Optional static TrueType fonts are loaded by case-insensitive filename search.
Place any supplied font binaries and their original SIL Open Font License and
upstream attribution files in this directory. Variable TTF files are not
supported by Dear ImGui's TrueType loader; the application warns and uses its
size-matched built-in font if no matching static font is available.

Interface font (Inter):
  Inter-Regular.ttf (required face; body/interface text)
  Inter-Medium.ttf (optional; compact labels)
  Inter-SemiBold.ttf (optional; titles and splash)

Data font (JetBrains Mono):
  JetBrainsMono-Regular.ttf (required face; measurements and readouts)
  JetBrainsMono-Medium.ttf (optional; emphasized data values)

The filename matching is case-insensitive and also accepts names containing
spaces, underscores, or hyphens. Use static, non-variable TTF files.
