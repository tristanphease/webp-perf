A repo for measuring the performance of webp decoding, especially for comparing libwebp against the rust implementation in image-webp

To profile, do `cargo build --profile profiling` then 
`samply record ./target/profiling/webp-perf.exe --in ./images/lossy/1.webp --decoder image-webp`

To benchmark do `cargo bench`

