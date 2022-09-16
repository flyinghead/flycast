cd "$(dirname "${BASH_SOURCE[0]}")"
index=1000
echo "#include <winresrc.h>" > ../core/gdxsv/textures.rc
echo "LANGUAGE LANG_ENGLISH,SUBLANG_NEUTRAL" >> ../core/gdxsv/textures.rc
for file in ../core/gdxsv/Textures/English/*.png ../core/gdxsv/Textures/English/**/*.png ../core/gdxsv/Textures/CommonUpscale/*.png ../core/gdxsv/Textures/CommonUpscale/**/*.png ; do
    if [ -e "$file" ]; then
        echo "$index PNG \"${file/\.\.\/core\/gdxsv/.}\"" >> ../core/gdxsv/textures.rc
        echo "$index 777 {\"$(basename "$file" .png)\"}" >> ../core/gdxsv/textures.rc
        (( index ++ ))
    fi
done
echo "LANGUAGE LANG_CHINESE,SUBLANG_NEUTRAL" >> ../core/gdxsv/textures.rc
for file in ../core/gdxsv/Textures/Cantonese/*.png ../core/gdxsv/Textures/Cantonese/**/*.png ../core/gdxsv/Textures/CommonUpscale/*.png ../core/gdxsv/Textures/CommonUpscale/**/*.png ; do
    if [ -e "$file" ]; then
        echo "$index PNG \"${file/\.\.\/core\/gdxsv/.}\"" >> ../core/gdxsv/textures.rc
        echo "$index 777 {\"$(basename "$file" .png)\"}" >> ../core/gdxsv/textures.rc
        (( index ++ ))
    fi
done
echo "LANGUAGE LANG_JAPANESE,SUBLANG_NEUTRAL" >> ../core/gdxsv/textures.rc
for file in ../core/gdxsv/Textures/Japanese/*.png ../core/gdxsv/Textures/Japanese/**/*.png ../core/gdxsv/Textures/CommonUpscale/*.png ../core/gdxsv/Textures/CommonUpscale/**/*.png ; do
    if [ -e "$file" ]; then
        echo "$index PNG \"${file/\.\.\/core\/gdxsv/.}\"" >> ../core/gdxsv/textures.rc
        echo "$index 777 {\"$(basename "$file" .png)\"}" >> ../core/gdxsv/textures.rc
        (( index ++ ))
    fi
done