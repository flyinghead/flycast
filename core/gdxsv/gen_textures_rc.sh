index=1000
echo "#include <winresrc.h>" > textures.rc
echo "LANGUAGE LANG_ENGLISH,SUBLANG_NEUTRAL" >> textures.rc
for file in ./Textures/English/*.png ./Textures/English/**/*.png ; do
    if [ -e "$file" ]; then
        echo "$index PNG \"$file\"" >> textures.rc
        filename="${d##*/}"
        echo "$index 777 {\"$(basename "$file" .png)\"}" >> textures.rc
        (( index ++ ))
    fi
done
echo "LANGUAGE LANG_CHINESE,SUBLANG_NEUTRAL" >> textures.rc
for file in ./Textures/Cantonese/*.png ./Textures/Cantonese/**/*.png ; do
    if [ -e "$file" ]; then
        echo "$index PNG \"$file\"" >> textures.rc
        filename="${d##*/}"
        echo "$index 777 {\"$(basename "$file" .png)\"}" >> textures.rc
        (( index ++ ))
    fi
done
echo "LANGUAGE LANG_JAPANESE,SUBLANG_NEUTRAL" >> textures.rc
for file in ./Textures/Japanese/*.png ./Textures/Japanese/**/*.png ; do
    if [ -e "$file" ]; then
        echo "$index PNG \"$file\"" >> textures.rc
        filename="${d##*/}"
        echo "$index 777 {\"$(basename "$file" .png)\"}" >> textures.rc
        (( index ++ ))
    fi
done