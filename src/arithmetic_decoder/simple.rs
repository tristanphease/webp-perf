//! A very simple decoder just as a simple way to understand how it works and as a baseline
//! Based on the old version of the decoder that was in the repository but annotated extensively
//! Note that that one's based on the decoder as described in the vp8 specification

use super::DecodingError;
use super::Prob;

/// A simple version of the arithmetic decoder.
/// 
/// This is basically as described in the vp8 specification. 
/// It works by keeping track of the current spot in the bytestream
/// and keeping track of the range and the value.  
/// 
/// e.g. if we have a range of 231 and a bool with probability
/// 30 (high chance of being 1), this gives a split as 1 + (231 - 1) * 30 / 256 = 27
/// since we're using an 16 bit value so we can decode a byte at a time instead of a bit at 
/// a time this means we want to use a split value as 27 * 2^8 = 6912 so
/// ------------------------------------------------------------------------------
/// 0 ------ 6912 -----------------------------------------------------------65535
/// ------------------------------------------------------------------------------
/// so if the current value is higher than the value then we're in the right side of the 
/// range so we need to return true and adjust the decoder by moving the left side to the split
/// value, so we subtract from the range and the current value accordingly
/// if the current value is less than the value then we're in the left side of the range so
/// we'll need to return false and adjust the decoder by moving the right side to the split value
/// this is just subtracting from the range since the current value won't have changed
pub struct SimpleArithmeticDecoder {
    /// The buffer of bytes to be decoded
    buf: Vec<u8>,
    /// The current index in the buffer that we're decoding
    index: usize,
    /// The range of probability values that the next bit can take
    /// Must be between 128 and 255
    range: u32,
    /// The value currently from the bitstream that will be read out soon
    value: u32,
    /// The number of bits shifted out of the value
    bit_count: u8,
}

impl SimpleArithmeticDecoder {
    pub(crate) fn new() -> Self {
        Self {
            buf: Vec::new(),
            range: 0,
            value: 0,
            bit_count: 0,
            index: 0,
        }
    }

    /// Initiates the decoder from a buffer of bytes
    pub(crate) fn init(&mut self, buf: Vec<u8>) -> Result<(), DecodingError> {
        if buf.len() < 2 {
            return Err(DecodingError::NotEnoughInitData.into());
        }

        self.buf = buf;
        // Direct access safe, since length has just been validated.
        // value needs to store at least a byte, we start by storing the 
        // first two bytes
        self.value = (u32::from(self.buf[0]) << 8) | u32::from(self.buf[1]);
        // index starting at the third byte since we have 2 bytes in the value already
        self.index = 2;
        // range starts as the whole range
        self.range = 255;
        // the number of bits starts at 0 since there's no bits decoded yet
        self.bit_count = 0;

        Ok(())
    }

    /// Reads a boolean with a certain probability
    pub(crate) fn read_bool(&mut self, probability: u8) -> bool {
        // the split using the current decoder and the probability provided
        // this is calculated as 1 + (range - 1) * prob / 256 
        // the idea of the split is that if the value is less than the split, we get
        // false, if the value is greater than or equal to the split, we get true. 
        let split = 1 + (((self.range - 1) * u32::from(probability)) >> 8);
        // multiply split by 256
        let bigsplit = split << 8;

        let retval = if self.value >= bigsplit {
            // we need to reduce the range
            self.range -= split;
            // subtract the left endpoint of the interval
            self.value -= bigsplit;
            true
        } else {
            // reduce the range to the split
            self.range = split;
            false
        };

        // if range is less than 128 we need to read more bytes until it's high enough
        // to read bits again
        while self.range < 128 {
            // multiply value and range by 2
            self.value <<= 1;
            self.range <<= 1;
            // move one bit along in the current byte
            self.bit_count += 1;

            // we're at the end of this byte so collect next byte
            if self.bit_count == 8 {
                // reset bits to start 
                self.bit_count = 0;

                // If no more bits are available, just don't do anything.
                // This strategy is suggested in the reference implementation of RFC6386 (p.135)
                if self.index < self.buf.len() {
                    // get value at current index and add it in bottom byte of value
                    self.value |= u32::from(self.buf[self.index]);
                    // move to next index for byte
                    self.index += 1;
                }
            }
        }

        retval
    }

    /// Reads a literal number from the decoder
    /// Note n must be 8 or less since we're only returning a byte from this function
    pub(crate) fn read_literal(&mut self, n: u8) -> u8 {
        let mut v = 0u8;
        for _ in 0..n {
            // each bit has a probability of 128 i.e. as likely to be a 0 as a 1
            v = (v << 1) + self.read_bool(128u8) as u8;
        }

        v
    }

    /// Reads a signed literal number from the decoder
    /// Note n must be 8 or less since we're only getting a byte as a literal
    pub(crate) fn read_magnitude_and_sign(&mut self, n: u8) -> i32 {
        // the magnitude is read first and it's from the n bits with probability half
        let magnitude = self.read_literal(n);
        // the sign is read as a flag; if it's true then will be negative 
        let sign = self.read_flag();

        if sign {
            -i32::from(magnitude)
        } else {
            i32::from(magnitude)
        }
    }

    /// Read from a tree using 
    /// A tree is defined as an array of signed values where if the value is 
    /// negative then it's a leaf node and the output value is the absolute value
    /// if the value is positive then it's an index into the tree for the next value 
    /// the probabilities is an array of half the size of the tree where the probability matches
    /// up to the corresponding branch node 
    pub(crate) fn read_with_tree(&mut self, tree: &[i8], probs: &[Prob], start: isize) -> i8 {
        // Can specify the start index of the tree so we set that here
        // 0 would be the start of the tree
        let mut index = start;

        // loop through until we find a leaf node 
        loop {
            // read a bool using the probability matching the tree provided
            let a = self.read_bool(probs[index as usize >> 1]);
            // if bool is true we look to the value at index + 1, otherwise look at the value at index
            let b = index + a as isize;
            // get the new value from the tree
            index = tree[b as usize] as isize;

            // if negative then this is the value we want
            if index <= 0 {
                break;
            }
        }

        // since value is negative we negate to get the correct value
        -index as i8
    }

    /// Simple helper method, a flag is just a literal with equal probability
    /// if the bit is 1 then true, if 0 then false
    pub(crate) fn read_flag(&mut self) -> bool {
        0 != self.read_literal(1)
    }
}

