import csv
import sys
import codecs

with codecs.open("../core/gdxsv/gdxsv_translation_patch.inc","w",encoding="shift_jis_2004") as f:
    sys.stdout = f
    print("""
//
//  gdxsv_translation_patch.inc
//  gdxsv
//
//  Created by Edward Li on 3/6/2021.
//  Copyright 2022 gdxsv. All rights reserved.
//
#define TRANSLATE(offset,length,original,cantonese,english) TranslationWithMaxLength<length>(offset,original,cantonese,english)
#define CUSTOMIZE(offset,length,original,cantonese,english,japanese) TranslationWithMaxLength<length>(offset,original,cantonese,english,japanese)

const static Translation translations[] = {
    """)
    with open('translation.csv') as csvfile:
        reader = csv.DictReader(csvfile)
        for row in reader:
            if row['Address'].startswith('//') == False:
                if row['Patched Japanese']:
                    print(f'    CUSTOMIZE({row["Address"]}, {row["MaxLength"]}, R"({row["Original"]})", R"({row["Cantonese"]})", R"({row["English"]})", R"({row["Patched Japanese"]})"),')
                else:
                    print(f'    TRANSLATE({row["Address"]}, {row["MaxLength"]}, R"({row["Original"]})", R"({row["Cantonese"]})", R"({row["English"]})"),')
    print("""
};
#undef TRANSLATE
#undef CUSTOMIZE

for (Translation translation : translations)
{
    const static u32 offset = 0x8C000000 + 0x00010000;
    const char * text = translation.Text();
    if (text)
    {
        const auto length = strlen(text);
        for (int i = 0; i < length; ++i)
        {
            WriteMem8_nommu(offset + translation.offset + i, u8(text[i]));
        }
        WriteMem8_nommu(offset + translation.offset + (u32)length, u8(0));
    }
}
    """)