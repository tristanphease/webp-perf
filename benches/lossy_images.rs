use std::{fs, hint::black_box, io::Cursor, path::PathBuf};

use criterion::{Criterion, criterion_group, criterion_main};
use image_webp::WebPDecodeOptions;

fn decode_lossy_image_webp(image_bytes: &[u8]) {
    let cursor = Cursor::new(image_bytes);
    let decode_options = WebPDecodeOptions::default();
    let mut decoder = image_webp::WebPDecoder::new_with_options(cursor, decode_options).unwrap();
    let mut image_buf = vec![0u8; decoder.output_buffer_size().unwrap()];
    decoder.read_image(&mut image_buf).unwrap();
}

fn decode_lossy_image_libwebp(image_bytes: &[u8]) {
    let decoder = webp::Decoder::new(image_bytes);
    let _image = decoder.decode().unwrap();
}

fn benchmark_lossy(c: &mut Criterion) {
    let files = get_files_in_dir("./images/lossy/");
    let mut group = c.benchmark_group("lossy_images");
    for lossy_file in files {
        let lossy_image = fs::read(&lossy_file).unwrap();
        let file_name: &str = lossy_file.file_name().unwrap().try_into().unwrap();
        group.bench_function(format!("{}_lossy_image_webp", file_name), |b| {
            b.iter(|| decode_lossy_image_webp(black_box(&lossy_image)))
        });
        group.bench_function(format!("{}_lossy_libwebp", file_name), |b| {
            b.iter(|| decode_lossy_image_libwebp(black_box(&lossy_image)))
        });
    }
    group.finish();
}

fn get_files_in_dir(directory: &str) -> impl Iterator<Item = PathBuf> {
    fs::read_dir(directory)
        .unwrap()
        .filter_map(|x| x.ok())
        .filter(|x| x.metadata().unwrap().is_file())
        .map(|x| x.path())
}

criterion_group!(benches, benchmark_lossy);
criterion_main!(benches);
