use std::{fs, io::Cursor};
use clap::{Parser, ValueEnum};

mod arithmetic_decoder;

fn main() {
    let args = Args::parse();

    let file = fs::read(&args.file_in)
        .expect("Couldn't find file to be decoded");
    match args.decoder {
        DecoderType::ImageWebP => decode_image_webp(&file, args.times),
        DecoderType::LibWebP => decode_image_libwebp(&file, args.times),
    }
}

fn decode_image_webp(file: &[u8], times: u32) {
    for _ in 0..times {
        let cursor = Cursor::new(file);
        let mut decoder = image_webp::WebPDecoder::new(cursor)
            .expect("Invalid webp file");
        let mut buffer = vec![0u8; decoder.output_buffer_size().unwrap()];
        decoder.read_image(&mut buffer).unwrap();
    }
}

fn decode_image_libwebp(file: &[u8], times: u32) {
    for _ in 0..times {
        let decoder = webp::Decoder::new(file);
        let _ = decoder.decode().unwrap();
    }
}

#[derive(Parser, Debug)]
#[command(version, about = "CLI for decoding webp images")]
struct Args {
    /// The file to be decoded
    #[arg(long = "in")]
    file_in: String,

    #[arg(short, long, default_value = "ImageWebP")]
    decoder: DecoderType,

    #[arg(short, long, default_value = "1")]
    times: u32,
}

#[derive(Clone, Copy, Debug, ValueEnum)]
enum DecoderType {
    #[value(name = "image-webp")]
    ImageWebP,
    #[value(name = "libwebp")]
    LibWebP
}
