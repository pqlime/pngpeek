# pngpeek
Simple image steganography tool for hiding messages with
minimal detection within images

# Build instructions
Just clone, configure cmake and `make build` / `make install`

```sh
cmake .
sudo make install
```

# Examples

## Hiding file "secret.txt" in image "image.png"
`pngpeek -e -i secret.txt -p image.png out.png`

## Extracting file "secret.txt" from image "out.png"
`pngpeek -p out.png secret.txt`