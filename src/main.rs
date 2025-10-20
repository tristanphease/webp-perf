use clap::{Parser, ValueEnum};
use std::{
    ffi::OsStr,
    fs,
    io::{self, Cursor},
    path::Path,
};

mod arithmetic_decoder;
mod transforms;

fn main() -> io::Result<()> {
    let args = Args::parse();

    let path = Path::new(&args.file_in);

    if path.is_file() {
        load_file_and_decode(path, args.decoder, args.times);
    } else if path.is_dir() {
        for path in std::fs::read_dir(path)?
            .filter_map(|x| x.ok())
            .map(|x| x.path())
            .filter(|x| x.extension() == Some(OsStr::new("webp")))
        {
            load_file_and_decode(&path, args.decoder, args.times);
        }
    } else {
        eprintln!("path not found or isn't valid");
    }

    Ok(())
}

fn load_file_and_decode<P>(file_name: P, decoder: DecoderType, times: u32)
where
    P: AsRef<Path>,
{
    let file = fs::read(file_name).expect("Couldn't find file to be decoded");
    match decoder {
        DecoderType::ImageWebP => decode_image_webp(&file, times),
        DecoderType::LibWebP => decode_image_libwebp(&file, times),
    }
}

fn decode_image_webp(file: &[u8], times: u32) {
    for _ in 0..times {
        let cursor = Cursor::new(file);
        let mut decoder = image_webp::WebPDecoder::new(cursor).expect("Invalid webp file");
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
    LibWebP,
}
