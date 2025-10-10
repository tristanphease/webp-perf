#![allow(dead_code)]

//! A place for me to experiment with the performance of the arithmetic decoder used in vp8/lossy
//! decoding. This is the most speed-critical part of the lossy decoder, making up something like
//! 50% of the whole time to decode some images
//! 
//! The vp8 specification has good detail of the theory and practice involved with the entropy coding
//! https://datatracker.ietf.org/doc/html/rfc6386#autoid-7 
//! The wikipedia article also shows some detail although it's more specific
//! https://en.wikipedia.org/wiki/Arithmetic_coding

mod image_webp;
mod simple;

// decoder error as in image-webp
#[derive(Debug)]
pub enum DecodingError {
    BitStreamError,
    NotEnoughInitData,
}

pub type Prob = u8;

// tree node used for decoding a tree as in image-webp
#[derive(Clone, Copy)]
pub(crate) struct TreeNode {
    pub left: u8,
    pub right: u8,
    pub prob: Prob,
    pub index: u8,
}

impl TreeNode {
    const UNINIT: TreeNode = TreeNode {
        left: 0,
        right: 0,
        prob: 0,
        index: 0,
    };

    const fn prepare_branch(t: i8) -> u8 {
        if t > 0 {
            (t as u8) / 2
        } else {
            let value = -t;
            0x80 | (value as u8)
        }
    }

    pub(crate) const fn value_from_branch(t: u8) -> i8 {
        (t & !0x80) as i8
    }
}

// generic trait for different decoder implementations
pub trait ArithmeticDecoder {

} 