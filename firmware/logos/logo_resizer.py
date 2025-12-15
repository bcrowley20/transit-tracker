from PIL import Image
import os

# Input folder with original logos
input_folder = r"C:\Users\bcrow\transit-tracker\firmware\logos\teams"

# Output folder for resized logos
output_folder = r"C:\Users\bcrow\transit-tracker\firmware\logos\m_resized"
os.makedirs(output_folder, exist_ok=True)

# Target sizes
sizes = [(14, 14)]

# Loop through all files in input folder
for filename in os.listdir(input_folder):
    if filename.lower().endswith((".png", ".jpg", ".jpeg")):
        filepath = os.path.join(input_folder, filename)
        img = Image.open(filepath)

        for size in sizes:
            resized = img.resize(size, Image.LANCZOS)
            base, ext = os.path.splitext(filename)
            new_name = f"{base}_{size[0]}x{size[1]}.png"  # always save as PNG
            resized.save(os.path.join(output_folder, new_name), "PNG")

print("Resizing complete! Logos saved in:", output_folder)
