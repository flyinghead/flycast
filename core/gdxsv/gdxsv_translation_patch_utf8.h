//
//  gdxsv_translation_patch.h
//  gdxsv
//
//  Created by Edward Li on 3/6/2021.
//  Copyright 2021 flycast. All rights reserved.
//
#define TRANSLATE(offset,length,original,cantonese,english) TranslationWithMaxLength<length>(offset,original,cantonese,english)
#define CUSTOMIZE(offset,length,original,cantonese,english,japanese) TranslationWithMaxLength<length>(offset,original,cantonese,english,japanese)

const u32 offset = 0x8C000000 + 0x00010000;

Translation translations[] = {

    //  Launch loading screen
    
    TRANSLATE(0x1BA264,28,
                R"(オートロード中です・・・)",
                R"(正在讀取存档・・・)",
                R"(Loading save file...)"
                ),
    TRANSLATE(0x1BA104,36,
                R"(メモリーカードがささっていません)",
                R"(未插入Sega VMU。 請到)",
                R"(Sega VMU is not inserted. Go to)"
                ),
    CUSTOMIZE(0x1BA128,20,
                R"(セーブを行うには)",
                R"(Flycast > Settings)",
                R"(Flycast > Settings)",
                R"(Flycast > Settings)"
                ),
    CUSTOMIZE(0x1BA13C,4,
                R"(4)",
                R"(　)",
                R"(　)",
                R"(　)"
                ),
    CUSTOMIZE(0x1BA140,12,
                R"(ブロックの)",
                R"( > Controls)",
                R"( > Controls)",
                R"( > Controls)"
                ),
    CUSTOMIZE(0x1BA14C,36,
                R"(空きがあるメモリーカードが必要です)",
                R"(Device A > 選擇 'Sega Controller')",
                R"(Device A > Select 'Sega Controller')",
                R"(Device A > セレクト Sega Controller)"
                ),
    CUSTOMIZE(0x1BA170,48,
                R"(スタートボタンを押すとゲームを開始できますが)",
                R"(在第二行，選擇 'Sega VMU')",
                R"(In the second column, select 'Sega VMU')",
                R"(2番目の列では, セレクト Sega VMU)"
                ),
    CUSTOMIZE(0x1BA1A0,40,
                R"(ゲームの内容はオートセーブされません)",
                R"(否則無法開始遊戲)",
                R"(Or else you cannot start the game)",
                R"(でないとゲームを開始できません。)"
                ),
    CUSTOMIZE(0x1BA1C8,40,
                R"(ＰＲＥＳＳ　ＳＴＡＲＴ　ＢＵＴＴＯＮ)",
                R"(　)",
                R"(　)",
                R"(　)"
                ),
    TRANSLATE(0x1BA1F0,32,
                R"(空きブロックが不足しています)",
                R"(沒有充足的記憶體)",
                R"(Not enough memory)"
                ),
    TRANSLATE(0x1BA210,44,
                R"(スタートボタンを押すとファイルを作成します)",
                R"(按ＳＴＡＲＴ掣建立存档)",
                R"(Press ＳＴＡＲＴ to create save file)"
                ),
    TRANSLATE(0x1BA2B8,32,
                R"(正しくロードできませんでした)",
                R"(無法正確讀取)",
                R"(Cannot load save file)"
                ),
    TRANSLATE(0x1BA2D8,44,
                R"(ファイルを作成中です・・・電源を切ったり)",
                R"(建立存档中・・・)",
                R"(Saving... Please do not power off)"
                ),
    TRANSLATE(0x1BA304,44,
                R"(メモリーカードを抜いたりしないでください)",
                R"(請勿關掉電源或取出記憶槽)",
                R"(or unplug memory card)"
                ),
    TRANSLATE(0x1BA330,24,
                R"(ファイルを作成しました)",
                R"(儲存成功)",
                R"(Memory is saved)"
                ),
    TRANSLATE(0x1BA348,32,
                R"(ファイルの作成に失敗しました)",
                R"(儲存失敗)",
                R"(Cannot save memory)"
                ),
    TRANSLATE(0x1BA368,40,
                R"(メモリーカードの接続を確認してください)",
                R"(請檢査記憶槽連接)",
                R"(Please verify memory connection)"
                ),
    TRANSLATE(0x1BA390,44,
                R"(スタートボタンを押すともう一度作成します)",
                R"(按ＳＴＡＲＴ掣再次建立存档)",
                R"(Press ＳＴＡＲＴ to recreate save file)"
                ),
    TRANSLATE(0x1BA3BC,28,
                R"(ファイルが破損しています)",
                R"(存档已損壞)",
                R"(Save file is corrupted)"
                ),
    TRANSLATE(0x1BA3D8,40,
                R"(オートセーブ中です・・・電源を切ったり)",
                R"(自動儲存中・・・)",
                R"(Auto saving... Please do not power off)"
                ),
    TRANSLATE(0x1BA400,24,
                R"(オートセーブしました)",
                R"(自動儲存成功)",
                R"(Auto saved)"
                ),
    TRANSLATE(0x1BA418,32,
                R"(正しくセーブできませんでした)",
                R"(儲存失敗)",
                R"(Cannot save memory)"
                ),
    TRANSLATE(0x1BA438,28,
                R"(オートセーブを解除します)",
                R"(取消自動儲存)",
                R"(Disable auto saving)"
                ),
    TRANSLATE(0x1BA454,24,
                R"(通信ゲームファイルを)",
                R"(連線遊戲存档)",
                R"(Online game save)"
                ),
    TRANSLATE(0x1BA46C,28,
                R"(すでにファイルが存在します)",
                R"(存档已經存在)",
                R"(Save file exists)"
                ),
    TRANSLATE(0x1BA488,20,
                R"(上書きしますか？)",
                R"(確認覆蓋？)",
                R"(Confirm overwrite?)"
                ),
    TRANSLATE(0x1BA49C,44,
                R"(ポート　　のメモリーカードが抜かれています)",
                R"(原本在　　的記憶槽已被移除)",
                R"(Slot　　　's memory card is removed)"
                ),
    TRANSLATE(0x1BA4C8,44,
                R"(オートセーブを解除してゲームを続けますか？)",
                R"(要取消自動儲存並繼續遊戲？)",
                R"(Disable auto saving and continue the game?)"
                ),
//    TRANSLATE(0x1BA4F4,24,
//              R"(通信ゲームファイルの)",
//              R"()",
//              R"()"),
//    TRANSLATE(0x1BA50C,4,
//              R"(９)",
//              R"(９)",
//              R"(９)"),
    TRANSLATE(0x1BA510,28,
                R"(オートセーブを解除しました)",
                R"(自動儲存已取消)",
                R"(Auto saving is disabled)"
                ),
    TRANSLATE(0x1BA52C,8,
                R"(はい)",
                R"(確定)",
                R"(Yes)"
                ),
    TRANSLATE(0x1BA534,12,
                R"(／いいえ)",
                R"(／取消)",
                R"(／No)"
                ),
    TRANSLATE(0x1BA540,8,
                R"(はい／)",
                R"(確定／)",
                R"(Yes／)"
                ),
    TRANSLATE(0x1BA548,8,
                R"(いいえ)",
                R"(取消)",
                R"(No)"
                ),
    
    TRANSLATE(0x1BA23C,40,
                R"(ポート　　のメモリーカードを使用します)",
                R"(將使用　　的記憶槽)",
                R"(Slot　　　's memory card will be used)"
                ),
    TRANSLATE(0x1BA280,24,
                R"(オートロードしました)",
                R"(自動讀取完成)",
                R"(Auto load completed)"
                ),
    TRANSLATE(0x1BA298,32,
                R"(以後ここにオートセーブします)",
                R"(以後亦會儲存到同一位置)",
                R"(Same slot will be used onwards)"
                ),

    // VMU operation
    
    TRANSLATE(0x1BE414,44,
                R"(セーブするメモリーカードを選んでください)",
                R"(請選擇要儲存的記憶槽)",
                R"(Please select your memory slot for saving)"
                ),
    TRANSLATE(0x1BE440,36,
                R"(メモリーカードがささっていません)",
                R"(未插入Sega VMU)",
                R"(Sega VMU is not inserted)"
                ),
    TRANSLATE(0x1BE464,32,
                R"(空きブロックが不足しています)",
                R"(沒有充足的記憶體)",
                R"(Not enough memory)"
                ),
    TRANSLATE(0x1BE484,32,
                R"(セーブを行うには４ブロックの)",
                R"(儲存遊戲需要4個記憶空間)",
                R"(4 blocks of memory is required)"
                ),
    TRANSLATE(0x1BE4A4,36,
                R"(空きがあるメモリーカードが必要です)",
                R"(　)",
                R"(to save the game)"
                ),
    TRANSLATE(0x1BE4C8,16,
                R"(セーブしました)",
                R"(儲存成功)",
                R"(Saved)"
                ),
    TRANSLATE(0x1BE4D8,28,
                R"(すでにファイルが存在します)",
                R"(存档已經存在)",
                R"(Save file exists)"
                ),
    TRANSLATE(0x1BE4F4,20,
                R"(上書きしますか？)",
                R"(確認覆蓋？)",
                R"(Confirm overwrite?)"
                ),
    TRANSLATE(0x1BE508,8,
                R"(はい)",
                R"(確定)",
                R"(Yes)"
                ),
    TRANSLATE(0x1BE510,12,
                R"(／いいえ)",
                R"(／取消)",
                R"(／No)"
                ),
    TRANSLATE(0x1BE51C,8,
                R"(はい／)",
                R"(確定／)",
                R"(Yes／)"
                ),
    TRANSLATE(0x1BE524,8,
                R"(いいえ)",
                R"(取消)",
                R"(No)"
                ),
    TRANSLATE(0x1BE52C,36,
                R"(セーブ中です・・・電源を切ったり)",
                R"(儲存中・・・請勿關機)",
                R"(Saving... Please do not power off)"
                ),
    TRANSLATE(0x1BE550,44,
                R"(メモリーカードを抜いたりしないでください)",
                R"(請勿關掉電源或取出記憶槽)",
                R"(or unplug memory card)"
                ),
    TRANSLATE(0x1BE57C,32,
                R"(以後ここにオートセーブします)",
                R"(以後亦會儲存到同一位置)",
                R"(Same slot will be used onwards)"
                ),
    TRANSLATE(0x1BE59C,32,
                R"(正しくセーブできませんでした)",
                R"(儲存失敗)",
                R"(Cannot save memory)"
                ),
    TRANSLATE(0x1BE5BC,28,
                R"(オートセーブを解除します)",
                R"(取消自動儲存)",
                R"(Cancel auto save)"
                ),
    TRANSLATE(0x1BE5D8,16,
                R"(ロードしました)",
                R"(讀取完成)",
                R"(Load completed)"
                ),
    TRANSLATE(0x1BE5E8,44,
                R"(ロードするメモリーカードを選んでください)",
                R"(請選擇要載入的記憶槽)",
                R"(Please select your memory slot for loading)"
                ),
    TRANSLATE(0x1BE614,32,
                R"(正しくロードできませんでした)",
                R"(無法正確讀取)",
                R"(Cannot load save file)"
                ),
    TRANSLATE(0x1BE634,28,
                R"(セーブファイルがありません)",
                R"(找不到遊戲存档)",
                R"(There is no save file)"
                ),
    TRANSLATE(0x1BE650,36,
                R"(ロード中です・・・電源を切ったり)",
                R"(載入中・・・請勿關機)",
                R"(Loading... Please do not power off)"
                ),
    TRANSLATE(0x1BE674,28,
                R"(ファイルが破損しています)",
                R"(請勿關掉電源或取出記憶槽)",
                R"(or unplug memory card)"
                ),
    TRANSLATE(0x1BE690,22,
                R"(年　月　日　時　分)",
                R"(年　月　日　時　分)",
                R"(／　／　　　：　　)"
                ),
    TRANSLATE(0x1BE6A4,32,
                R"(セーブを行うには９ブロックの)",
                R"(儲存連線遊戲需要9個記憶空間)",
                R"(9 blocks of memory is required)"
                ),
    TRANSLATE(0x1BE6C4,32,
                R"(通信ゲームファイルがありません)",
                R"(找不到連線存档)",
                R"(There is no online game save)"
                ),
    TRANSLATE(0x1BE6E4,20,
                R"(プレーヤー１だよ)",
                R"(Player 1)",
                R"(Player 1)"
                ),
    TRANSLATE(0x1BE6F8,20,
                R"(プレーヤー２だよ)",
                R"(Player 2)",
                R"(Player 2)"
                ),
    TRANSLATE(0x1BE70C,20,
                R"(プレーヤー３だよ)",
                R"(Player 3)",
                R"(Player 3)"
                ),
    TRANSLATE(0x1BE720,20,
                R"(プレーヤー４だよ)",
                R"(Player 4)",
                R"(Player 4)"
                ),

    // Ranking

    TRANSLATE(0x1BE870,8,
                R"(二等兵)",
                R"(二等兵)",
                R"(PSC)"
                ),
    TRANSLATE(0x1BE878,8,
                R"(一等兵)",
                R"(一等兵)",
                R"(PFC)"
                ),
    TRANSLATE(0x1BE880,8,
                R"(上等兵)",
                R"(上等兵)",
                R"(L/CPL)"
                ),
    TRANSLATE(0x1BE888,8,
                R"(伍　長)",
                R"(伍　長)",
                R"(CPL)"
                ),
    TRANSLATE(0x1BE890,8,
                R"(軍　曹)",
                R"(軍　曹)",
                R"(SGT)"
                ),
    TRANSLATE(0x1BE898,8,
                R"(曹　長)",
                R"(曹　長)",
                R"(SGM)"
                ),
    TRANSLATE(0x1BE8A0,8,
                R"(少　尉)",
                R"(少　尉)",
                R"(2.LT)"
                ),
    TRANSLATE(0x1BE8A8,8,
                R"(中　尉)",
                R"(中　尉)",
                R"(1.LT)"
                ),
    TRANSLATE(0x1BE8B0,8,
                R"(大　尉)",
                R"(大　尉)",
                R"(CAP)"
                ),
    TRANSLATE(0x1BE8B8,8,
                R"(少　佐)",
                R"(少　佐)",
                R"(MAJ)"
                ),
    TRANSLATE(0x1BE8C0,8,
                R"(中　佐)",
                R"(中　佐)",
                R"(L/COL)"
                ),
    TRANSLATE(0x1BE8C8,8,
                R"(大　佐)",
                R"(大　佐)",
                R"(COL)"
                ),
    TRANSLATE(0x1BE8D0,8,
                R"(少　将)",
                R"(少　將)",
                R"(M/GEN)"
                ),
    TRANSLATE(0x1BE8D8,8,
                R"(中　将)",
                R"(中　將)",
                R"(L/GEN)"
                ),
    TRANSLATE(0x1BE8E0,8,
                R"(大　将)",
                R"(大　將)",
                R"(GEN)"
                ),
    TRANSLATE(0x1BE8E8,8,
                R"(さん)",
                R"(San)",
                R"(San)"
                ),
    //    TRANSLATE(0x1BE8F0,16,
    //              R"(なし)",
    //              R"(None)",
    //              R"(None)"
    //              ),
        
//    TRANSLATE(0x1BE900,20,
//              R"(課金対象電話番号)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BE914,8,
//              R"(氏名)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BE91C,8,
//              R"(年齢)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BE924,12,
//              R"(郵便番号)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BE930,8,
//              R"(住所)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BE938,24,
//              R"(は必ず入力して下さい)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1BE950,36,
                R"(この内容で登録してよろしいですか？)",
                R"(        確定要註冊此内容？)",
                R"(Confirm registering with this info?)"
                ),
    TRANSLATE(0x1BE974,8,
                R"(はい)",
                R"(確定)",
                R"( Yes)"
                ),
    TRANSLATE(0x1BE97C,8,
                R"(いいえ)",
                R"( 取消)",
                R"(  No)"
                ),
//    TRANSLATE(0x1BE984,36,
//              R"(ダウンロードしてもよろしいですか？)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BE9A8,8,
//              R"(ＩＤ：)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BE9B0,28,
//              R"(はすでに登録されています)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BE9CC,8,
//              R"(ダミー)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BE9D4,20,
//              R"(ダウンロード中です)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BE9E8,100,
//              R"(接続に失敗しました)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BE9FC,24,
//              R"(接続できませんでした)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEA14,24,
//              R"(モジュラーケーブルが)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEA2C,24,
//              R"(本体につながっているか)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEA44,40,
//              R"(モデムの設定が正しいか確認して下さい)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEA6C,32,
//              R"(正しい設定であるにもかかわらず)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEA8C,20,
//              R"(反応がない場合は)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEAA0,36,
//              R"(カプコンユーザーサポートセンターに)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEAC4,20,
//              R"(お問い合わせ下さい)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEAD8,28,
//              R"(ダウンロードが失敗しました)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEAF4,12,
//              R"(話し中です)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEB00,28,
//              R"(しばらく時間をおいて下さい)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEB1C,24,
//              R"(モデムの設定が正しいか)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEB34,16,
//              R"(確認して下さい)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEB44,32,
//              R"(システムデータが複数あります)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEB64,36,
//              R"(どのポートのデータを使用しますか？)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEB88,8,
//              R"(Ａ−１)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEB90,8,
//              R"(Ａ−２)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEB98,32,
//              R"(ただいま対戦相手を捜しています)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEBB8,24,
//              R"(しばらくお待ち下さい)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEBD0,28,
//              R"(ダウンロードが終了しました)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEBEC,20,
//              R"(対戦を断られました)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEC00,28,
//              R"(ＫＤＤＩ回線で接続中です)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEC1C,40,
//              R"(対戦中は電源を切ったりしないで下さい)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1BEC44,36,
                R"(回線が不安定なため切断されました)",
                R"(     網絡不穩定，已中斷連線)",
                R"(Disconnected due to unstable line)"
                ),
    TRANSLATE(0x1BEC68,24,
                R"(Ａボタンを押して下さい)",
                R"(       請按Ａ掣)",
                R"(Please press Button A)"
                ),
    TRANSLATE(0x1BEC80,36,
                R"(ハンドルネームが入力されていません)",
                R"(          未輸入網絡名稱)",
                R"(    Handle Name cannot be blank)"
                ),
    TRANSLATE(0x1BECA4,36,
                R"(このハンドルネームで登録しますか？)",
                R"(        確認使用此網絡名稱？)",
                R"(  Confirm using this Handle Name?)"
                ),
//    TRANSLATE(0x1BECC8,16,
//              R"(を登録しました)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BECD8,24,
//              R"(を登録できませんでした)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BECF0,32,
//              R"(これ以上ＩＤを登録できません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BED10,32,
//              R"(ＩＤの編集をおこなって下さい)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BED30,20,
//              R"(登録が完了しました)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BED44,24,
//              R"(あなたのカプコンＩＤは)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BED5C,8,
//              R"(です)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1BED64,28,
                R"(マルチマッチングサーバーに)",
                R"(     GDXSV 免費SERVER)",
                R"(GDXSV FREE OF CHARGE SERVER)"
                ),
//    TRANSLATE(0x1BED80,12,
//              R"(接続中です)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BED8C,36,
//              R"(スタートボタンを１秒間以上押すと)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEDB0,20,
//              R"(キャンセルします)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEDC4,26,
//              R"(最新の通信ゲーム利用規約に)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEDE0,16,
//              R"(同意しますか？)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEDF0,28,
//              R"(は現在、通信対戦サーバーに)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BEE0C,32,
//              R"(接続していないか、対戦中です)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1BEE2C,32,
                R"(接続中は通信料金がかかります)",
                R"(  原來撥號連線所産生的費用)",
                R"( Original charges by dial-up)"
                ),
    TRANSLATE(0x1BEE4C,36,
                R"(マルチマッチングサーバーに接続中は)",
                R"(         連接到遊戲大廳時)",
                R"(  while connected to lobby server)"
                ),
    TRANSLATE(0x1BEE70,32,
                R"(３分１０円の接続料がかかります)",
                R"(        毎３分鐘１０円)",
                R"(       ¥10 per 3 minutes)"
                ),
    TRANSLATE(0x1BEE90,12,
                R"(対戦時には)",
                R"(  對戰時)",
                R"(For battle)"
                ),
    TRANSLATE(0x1BEE9C,32,
                R"(１分１３円の接続料がかかります)",
                R"(        毎１分鐘１３円)",
                R"(       ¥13 per 1 minutes)"
                ),
    TRANSLATE(0x1BEEBC,20,
                R"(接続しますか？)",
                R"(   是否連接？)",
                R"(   Connect?)"
                ),
//    TRANSLATE(0x1BEED0,32,
//              R"(メモリーカードをチェック中です)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1BEEF0,32,
                R"(電源を切ったりメモリーカードを)",
                R"(     請勿關機或移除記憶槽)",
                R"( Do not turn off Dreamcast or)"
                ),
    TRANSLATE(0x1BEF10,24,
                R"(抜いたりしないで下さい)",
                R"( )",
                R"(or remove memory card)"
                ),
//    TRANSLATE(0x1BEF28,32,
//              R"(正しくロードできませんでした)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1BEF48,12,
                R"(操作説明)",
                R"(操作説明)",
                R"(How to use)"
                ),
    TRANSLATE(0x1BEF54,24,
                R"(方向ボタン ：上下左右)",
                R"(方向掣   ：上下左右)",
                R"(DPad: Direction)"
                ),
    TRANSLATE(0x1BEF6C,20,
                R"(Ａボタン   ：決定)",
                R"(Ａ掣     ：決定)",
                R"(A: OK)"
                ),
    TRANSLATE(0x1BEF80,24,
                R"(Ｂボタン   ：キャンセル)",
                R"(Ｂ掣     ：取消)",
                R"(B: Cancel)"
                ),
    TRANSLATE(0x1BEF98,36,
                R"(Ｙボタン   ：ソフトキーボード表示)",
                R"(Ｙ掣     ：顯示螢幕小鍵盤)",
                R"(Y: Show On-Screen Keyboard)"
                ),
    TRANSLATE(0x1BEFBC,24,
                R"(Ｘボタン   ：過去ログ)",
                R"(Ｘ掣     ：聊天紀録)",
                R"(X: Show Chatlog)"
                ),
    TRANSLATE(0x1BEFD4,24,
                R"(           ：ページ送り)",
                R"( )",
                R"( )"
                ),
    TRANSLATE(0x1BEFEC,24,
                R"(STARTボタン：閉じる )",
                R"(START掣：關閉)",
                R"(START: Close)"
                ),
//    TRANSLATE(0x1BF004,24,
//              R"(のＩＤを登録しますか？)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1BF01C,20,
                R"(ゲームに戻ります)",
                R"( 返回遊戲主目録)",
                R"(Back to main menu)"
                ),
    TRANSLATE(0x1BF030,36,
                R"(回線を切断してもよろしいですか？)",
                R"(         確認中斷連線？)",
                R"(     Confirm disconnecting?)"
                ),
//    TRANSLATE(0x1BF054,32,
//              R"(正しくセーブできませんでした)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF074,28,
//              R"(登録済みのメモリーカードを)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF090,32,
//              R"(ポート　　　に接続して下さい)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF0B0,32,
//              R"(カプコンサーバーと交信中です)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF0D0,24,
//              R"(モデムが見つかりません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF0E8,24,
//              R"(モデムが外れていないか)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF100,36,
//              R"(通信ゲーム利用規約に同意しない場合)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF124,36,
//              R"(通信対戦を利用する事ができませんが)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF148,24,
//              R"(よろしいでしょうか？)",
//              R"( )",
//              R"( )"
//              ),
    CUSTOMIZE(0x1BF160,40,
                R"(本体に電話回線が接続されていることを)", 
                R"(1.為確保連線質素,請使用LAN線進行遊戲)",
                R"(1.Use wired connection for best exp.)",
                R"(1.接続品質を維持するため、有線接続で)"
                ),
    CUSTOMIZE(0x1BF188,20,
                R"(確認してください)",
                R"(用Wi-Fi會超級窒!!!)",
                R"(Wi-Fi is laggy!)",
                R"(ご利用ください)"
                ),
    CUSTOMIZE(0x1BF19C,36,
                R"(電話回線契約者（ご家族の方等）の)",
                R"(閣下可能唔覺窒，但係其他3個人就)",
                R"(Even if you don't feel it, other)",
                R"(Wi-Fiを使用する場合、接続速度に影響)"
                ),
    CUSTOMIZE(0x1BF1C0,24,
                R"(承諾を得ていますか？)",
                R"(窒到呀媽都唔認得 :0))",
                R"(players'll be affected.)",
                R"(    があります)"
                ),
    CUSTOMIZE(0x1BF1D8,28,
                R"(通信対戦をプレイするには)",
                R"(2.開始對戰後,千祈唔好閂game)",
                R"(2.Do not disconnect after)",
                R"(2.戦闘開始後にFlycastを終了)"
                ),
    CUSTOMIZE(0x1BF1F4,24,
                R"(通信ゲーム利用規約に)",
                R"(要出番去大廳先可以閂)",
                R"(battle has started)",
                R"(させないでください)"
                ),
    CUSTOMIZE(0x1BF20C,24,
                R"(同意する必要があります)",
                R"(3.不teamkill，不棄game)",
                R"(3.No TK, play seriously)",
                R"(3.チームキル/放棄なし)"
                ),
    CUSTOMIZE(0x1BF224,40,
                R"(通信ゲーム利用規約を確認しましたか？)",
                R"(雖然打法唔同,但請尊重各國玩家,讓下新手)",
                R"(Respect all players around the world)",
                R"(グローバルプレイヤーを尊重してください)"
                ),
    TRANSLATE(0x1BF24C,12,
                R"(同意する)",
                R"(同意)",
                R"(Agree)"
                ),
    TRANSLATE(0x1BF258,12,
                R"(同意しない)",
                R"(不同意)",
                R"(Disagree)"
                ),
//    TRANSLATE(0x1BF264,16,
//              R"(モデム認識中)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1BF274,32,
                R"(サーバーに接続できませんでした)",
                R"(未能連接到server)",
                R"(Could not connect to server)"
                ),
//    TRANSLATE(0x1BF294,28,
//              R"(サーバーがメンテナンス中の)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF2B0,20,
//              R"(可能性もあります)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1BF2C4,28,
                R"(一定時間入力が無かったため)",
                R"( )",
                R"(   No input for too long)"
                ),
    TRANSLATE(0x1BF2E0,20,
                R"(回線を切断しました)",
                R"(  連線已被自動中斷)",
                R"(Auto-disconnected)"
                ),
    TRANSLATE(0x1BF2F4,24,
                R"(通信不良が発生しました)",
                R"(   連線不穩定而中斷)",
                R"( Unstable connection)"
                ),
    TRANSLATE(0x1BF30C,32,
                R"(対戦を終了しロビーに戻ります)",
                R"( 對戰結束，將會返回遊戲大廳)",
                R"( Match Ended. Back to Lobby)"
                ),
//    TRANSLATE(0x1BF32C,32,
//              R"(通信ゲームファイルが登録された)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF34C,20,
//              R"(メモリーカードを)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF360,16,
//              R"(ポート　　　に)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF370,28,
//              R"(９ブロック以上空きがある)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF38C,32,
//              R"(メモリーカードを接続して下さい)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF3AC,36,
//              R"(登録内容を変更してよろしいですか？)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF3D0,40,
//              R"(通信ゲームファイルは対応するゲームで)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF3F8,40,
//              R"(共通して使えます)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF40C,36,
//              R"(対応している接続機器（モデム）が)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF430,16,
//              R"(見つかりません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF440,24,
//              R"(解説書に表記されている)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF458,24,
//              R"(モデムをご使用下さい)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF470,28,
//              R"(他のモデムを使用する場合は)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF48C,28,
//              R"(１度本体の電源を切ってから)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF4A8,32,
//              R"(「通信対戦」を選択して下さい)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1BF4C8,24,
                R"(通信ゲームファイルに)",
                R"( 正在保存相關設定到)",
                R"(Saving the settings)"
                ),
    TRANSLATE(0x1BF4E0,24,
                R"(設定を保存しています)",
                R"(    連線遊戲存档)",
                R"(to online game save)"
                ),
    TRANSLATE(0x1BF4F8,16,
                R"(セーブしました)",
                R"(   儲存成功)",
                R"(    Saved)"
                ),
//    TRANSLATE(0x1BF508,16,
//              R"(ロードしました)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF518,8,
//              R"(移動中)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF520,24,
//              R"(対戦を申し込んでいます)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF538,20,
//              R"(接続を中断しました)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1BF54C,36,
                R"(通信ゲームファイルをロードします)",
                R"(載入連線遊戲存档)",
                R"(Load the online game save)"
                ),
    TRANSLATE(0x1BF570,32,
                R"(ファイルをロードしない場合は)",
                R"(如不載入，連線相關的資料)",
                R"(If you do not load the file,)"
                ),
    TRANSLATE(0x1BF590,36,
                R"(モデム設定や他プレイヤーのＩＤ等が)",
                R"(例如modem設定、其他玩家的ID等)",
                R"(modem settings, other players' IDs,)"
                ),
    TRANSLATE(0x1BF5B4,36,
                R"(セーブできませんのでご注意ください)",
                R"(將不會被儲存。)",
                R"(etc. will not be saved.)"
                ),
    TRANSLATE(0x1BF5D8,40,
                R"(通信ゲームファイルをロードしますか？)",
                R"(是否載入？)",
                R"(Do you want to load the file?)"
                ),
//    TRANSLATE(0x1BF600,28,
//              R"(「通信対戦」モード終了まで)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF61C,24,
//              R"(メモリーカードを絶対に)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF634,20,
//              R"(抜かないで下さい)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF648,28,
//              R"(通信対戦を終了しますか？)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF664,16,
//              R"(ポートＡ−１に)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF674,16,
//              R"(見つかりました)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF684,20,
//              R"(通信ゲーム可能です)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF698,20,
//              R"(これ以降はモデムを)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF6AC,24,
//              R"(抜き差ししないで下さい)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF6C4,32,
//              R"(のＩＤを削除してもいいですか？)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF6E4,32,
//              R"(あなたのＩＤや通信相手のＩＤ)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF704,32,
//              R"(などメモリーカードの登録情報を)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF724,32,
//              R"(全て消去してもよろしいですか？)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF744,32,
//              R"(元のメモリーカードを接続して)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF764,28,
//              R"(正しくセーブされないので)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF780,16,
//              R"(先に進めません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF790,32,
//              R"(元のメモリーカードがないので)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF7B0,16,
//              R"(接続できません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF7C0,28,
//              R"(このモードを終了しますか？)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF7DC,20,
//              R"(登録しませんでした)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF7F0,20,
//              R"(はビジー状態です)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF804,20,
//              R"(しばらくしてから)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF818,28,
//              R"(再度チャレンジして下さい)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF834,32,
//              R"(モデムからの応答がありません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF854,24,
//              R"(データをロードします)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF86C,32,
//              R"(メモリーカードを拡張ソケットに)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF88C,16,
//              R"(入れて下さい)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF89C,32,
//              R"(                      Ａ−１)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF8BC,36,
//              R"(                      Ａ−１か２)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF8E0,24,
//              R"(ＮＥＴファイル      ＝)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF8F8,24,
//              R"(システムファイル　　＝)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF910,16,
//              R"(準備ができたら)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF920,32,
//              R"(スタートボタンを押して下さい)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF940,24,
//              R"(データをロードしました)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF958,28,
//              R"(のデータが設定されました)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF974,28,
//              R"(入室条件を満たしていません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF990,28,
//              R"(この対戦ルームは満員です)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF9AC,40,
//              R"(サーバーが混雑しているかもしれません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF9D4,32,
//              R"(モデムの設定が正しいか確認して)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BF9F4,28,
//              R"(もう一度おかけなおし下さい)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFA10,32,
//              R"(システムファイルがありません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFA30,12,
//              R"(Ａ−１か２)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFA3C,36,
//              R"(          にシステムファイルの入った)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFA64,24,
//              R"(メモリーカードを入れて)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFA7C,32,
//              R"(モデムの設定が間違っているか)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFA9C,28,
//              R"(接続先が大変混み合っており)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFAB8,20,
//              R"(電話がかかりません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFACC,20,
//              R"(再接続しますか？)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFAE0,24,
//              R"(相手の反応がありません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFAF8,36,
//              R"(何らかの原因で接続できませんでした)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFB1C,12,
//              R"(サーバーが)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFB28,28,
//              R"(混雑しているかもしれません)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1BFB44,24,
                R"(サーバーに接続中です)",
                R"( 正在連線到遊戲大廳)",
                R"( Connecting to lobby)"
                ),
//    TRANSLATE(0x1BFB5C,24,
//              R"(回線が混雑しています)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFB74,28,
//              R"(次の番号でかけ直しますか？)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFB90,32,
//              R"(電話番号が登録されていません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFBB0,32,
//              R"(前回の戦いのリプレイデータを)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFBD0,28,
//              R"(ファイルにセーブしますか？)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFBEC,24,
//              R"(セーブを終了しますか？)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFC04,20,
//              R"(上書きしますか？)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFC18,24,
//              R"(新規でセーブしますか？)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFC30,40,
//              R"(Ａ-１にささっているメモリーカードに)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFC58,32,
//              R"(ＮＥＴファイルを新規作成します)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFC78,20,
//              R"(よろしいですか？)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFC8C,36,
//              R"(は現在、この対戦ルームにはいません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFCB0,32,
//              R"(はメッセージを受け取りました)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1BFCD0,16,
                R"(メール送信中)",
                R"(   傳送中)",
                R"(  Sending)"
                ),
//    TRANSLATE(0x1BFCE0,36,
//              R"(のメールを次回から拒否しますか？)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFD04,36,
//              R"(のメールは次回から受け取りません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFD28,32,
//              R"(の対戦を次回から拒否しますか？)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFD48,28,
//              R"(の対戦を次回から拒否します)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFD64,36,
//              R"(これよりトーナメントを開始します)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFD88,28,
//              R"(トーナメント情報取得中です)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFDA4,32,
//              R"(しばらくそのままでお待ち下さい)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFDC4,36,
//              R"(相手がいないため不戦勝になりました)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1BFDE8,20,
                R"(対戦を開始します)",
                R"(   對戰開始)",
                R"( Battle Begins)"
                ),
//    TRANSLATE(0x1BFDFC,24,
//              R"(次の対戦を開始します)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFE14,36,
//              R"(只今、トーナメントが行われています)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFE38,36,
//              R"(時間をおいて再エントリーして下さい)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFE5C,36,
//              R"(トーナメントは開催されていません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFE80,40,
//              R"(エントリー条件が満たされていないため)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFEA8,24,
//              R"(エントリーできません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFEC0,24,
//              R"(全試合が終了しました)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFED8,20,
//              R"(お疲れさまでした)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFEEC,36,
//              R"(これでトーナメントは終了いたします)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFF10,32,
//              R"(またの参加をお待ちしています)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFF30,40,
//              R"(一定時間でサーバーに戻れなかったので)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFF58,20,
//              R"(失格となりました)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFF6C,24,
//              R"(参加者が足りませんので)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFF84,36,
//              R"(トーナメントは不成立になりました)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFFA8,28,
//              R"(優勝者が決まりませんでした)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFFC4,24,
//              R"(登録拒否数オーバーです)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFFDC,32,
//              R"(これ以上、拒否者登録できません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1BFFFC,32,
//              R"(ギャラリーモードで入りました)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C001C,40,
//              R"(開催中のトーナメントの経過が見れます)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0044,32,
//              R"(トーナメントへの参加希望の方は)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0064,24,
//              R"(次回開催までお待ち下さい)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0080,36,
//              R"(トーナメントモードを退場しますか？)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C00A4,28,
//              R"(一度退場すると次回開催まで)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C00C0,24,
//              R"(入場できなくなります)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C00D8,8,
                R"(検索中)",
                R"(搜尋中)",
                R"(Finding)"
                ),
//    TRANSLATE(0x1C00E0,4,
//              R"(は)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C00E4,12,
//              R"(にいます)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C00F0,28,
//              R"(メモリーカード差込口　の)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C010C,24,
//              R"(メモリーカード(PS2)が )",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0124,28,
//              R"(フォーマットされていません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0140,24,
//              R"(フォーマットしますか？)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0158,24,
//              R"(メモリーカード(PS2)を )",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0170,28,
//              R"(フォーマットしています…)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C018C,16,
//              R"(電源を切ったり)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C019C,28,
//              R"(抜き差ししないでください)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C01B8,36,
//              R"(正しくフォーマットできませんでした)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C01DC,28,
//              R"(ハードディスクドライブに)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C01F8,24,
//              R"(エラーが発生しました)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0210,32,
//              R"(ハードディスクドライブ付属の)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0230,28,
//              R"(取扱説明書の指示に従って)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C024C,24,
//              R"(修復を行ってください)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0264,28,
//              R"(１２ブロック以上空きがある)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0280,32,
//              R"(システムファイルが登録された)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C02A0,24,
//              R"(メモリーカード(PS2)と )",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C02B8,28,
//              R"(ハードディスクドライブが)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C02D4,24,
//              R"(見つかりませんでした)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C02EC,32,
//              R"(通信ゲームファイルを利用せずに)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C030C,20,
//              R"(先へ進みますか？)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0320,24,
//              R"(通信ゲームファイルが)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0338,28,
//              R"(ファイルを作成しない場合は)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0354,20,
//              R"(新規作成しますか？)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0368,28,
//              R"(ハードディスクドライブを)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0384,24,
//              R"(チェックしています…)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C039C,24,
//              R"(通信ゲームファイルの)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C03B4,24,
//              R"(設定を保存しています…)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C03CC,32,
//              R"(電源を切ったりしないで下さい)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C03EC,24,
//              R"(通信ゲームファイルを)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0404,24,
//              R"(○ボタンを押して下さい)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C041C,28,
//              R"(メモリーカード(PS2)には )",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0438,32,
//              R"(通信ゲームファイルがありません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0458,28,
//              R"(メモリーカード差込口　に)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0474,16,
//              R"(接続して下さい)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0484,24,
//              R"(○ボタン＝セーブしない)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C049C,24,
                R"(メールを送信しました)",
                R"(       已傳送)",
                R"(     Email sent)"
                ),
    TRANSLATE(0x1C04B4,24,
                R"(対戦した相手のＩＤを)",
                R"( 閣下想把對手的ＩＤ)",
                R"(Do you want to add the)"
                ),
    TRANSLATE(0x1C04CC,32,
                R"(戦友ファイルに記録しますか？)",
                R"(    加入到戰友名單當中？)",
                R"(opponents in your friend list?)"
                ),
    TRANSLATE(0x1C04EC,16,
                R"(登録しました)",
                R"(   已加入)",
                R"(   Added)"
                ),
    TRANSLATE(0x1C04FC,24,
                R"(情報を取得しています)",
                R"(    正在收集情報)",
                R"(Info is being acquired)"
                ),
    TRANSLATE(0x1C0514,24,
                R"(しばらくお待ちください)",
                R"(      請耐心等候)",
                R"( Please wait a moment)"
                ),
    TRANSLATE(0x1C052C,32,
                R"(がパートナーを名乗り出ています)",
                R"(     想成為閣下的PARTNER)",
                R"(   Wants to be your partner)"
                ),
    TRANSLATE(0x1C054C,36,
                R"(をパートナーとして了解しますか？)",
                R"(     接受成為閣下的PARTNER？)",
                R"(Do you accept him as your partner?)"
                ),
    TRANSLATE(0x1C0570,16,
                R"(拒否しました)",
                R"(   已拒絶)",
                R"(  Refused)"
                ),
    TRANSLATE(0x1C0580,24,
                R"(戦績データを更新します)",
                R"(      戰績已更新)",
                R"( Battle record updated)"
                ),
    TRANSLATE(0x1C0598,16,
                R"(は去りました)",
                R"(   已離開)",
                R"(  has left)"
                ),
//    TRANSLATE(0x1C05A8,16,
//              R"(相手がいません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C05B8,24,
//              R"(ただ今、混雑しています)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C05D0,20,
//              R"(時間をおいてから)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C05E4,28,
//              R"(もう一度確認してください)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C0600,20,
                R"(定員が埋まりました)",
                R"(     隊伍已滿)",
                R"( Slot's been filled)"
                ),
    TRANSLATE(0x1C0614,32,
                R"(他のパートナーを探してください)",
                R"(       請尋找其他PARTNER)",
                R"( Please find another partner)"
                ),
//    TRANSLATE(0x1C0634,16,
//              R"(ＢＵＳＹです)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0644,24,
//              R"(相手と条件が違うために)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C065C,20,
//              R"(チームを組めません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0670,24,
//              R"(のＩＤを削除しました)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0688,32,
//              R"(全ての項目を入力してください)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C06A8,28,
//              R"(必須項目を入力してください)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C06C4,24,
                R"(メールを送信しますか？)",
                R"(    確定傳送信息？)",
                R"(Confirm sending mail?)"
                ),
//    TRANSLATE(0x1C06DC,36,
//              R"(相手のゲームとタイトルが違うために)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0700,36,
//              R"(セーブをせずにゲームを続けますか？)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C0724,12,
                R"(戦場移動)",
                R"(選擇戰場)",
                R"(Battlefield)"
                ),
    TRANSLATE(0x1C0730,12,
                R"(発言者一覧)",
                R"(發言者名單)",
                R"(Speakers)"
                ),
    TRANSLATE(0x1C073C,16,
                R"(特定の人を捜す)",
                R"(搜尋指定對象)",
                R"(ID Lookup)"
                ),
    TRANSLATE(0x1C074C,16,
                R"(メールを開く)",
                R"(打開郵箱)",
                R"(Open Mailbox)"
                ),
    TRANSLATE(0x1C075C,12,
                R"(対戦成績)",
                R"(對戰成績)",
                R"(Ranking)"
                ),
    TRANSLATE(0x1C0768,12,
                R"(ネット終了)",
                R"(中斷連線)",
                R"(Disconnect)"
                ),
//    TRANSLATE(0x1C0774,12,
//              R"(ＥＸＩＴ)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C0780,28,
                R"(戦場選択画面に移動します)",
                R"(前往戰場選擇畫面)",
                R"(Go to battlefield selection)"
                ),
//    TRANSLATE(0x1C079C,4,
//              R"(　)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C07A0,36,
                R"(発言したユーザーの一覧を表示します)",
                R"(査閲發言者名單)",
                R"(Display speaker list)"
                ),
    TRANSLATE(0x1C07C4,20,
                R"(メールを送ったり、)",
                R"(傳送信息、)",
                R"(Send mail,)"
                ),
    TRANSLATE(0x1C07D8,32,
                R"(他のユーザーの検索ができます)",
                R"(或搜尋其他玩家)",
                R"(or search for other players)"
                ),
    TRANSLATE(0x1C07F8,20,
                R"(メールを表示します)",
                R"(査閲信息)",
                R"(Check for mail)"
                ),
    TRANSLATE(0x1C080C,24,
                R"(対戦成績を表示します)",
                R"(顯示個人的對戰成績)",
                R"(Show the battle results)"
                ),
    TRANSLATE(0x1C0824,24,
                R"(操作ボタンを説明します)",
                R"(顯示操作説明)",
                R"(Show tutorial)"
                ),
    TRANSLATE(0x1C083C,20,
                R"(回線を切断して、)",
                R"(中斷連線)",
                R"(Disconnect and)"
                ),
    TRANSLATE(0x1C0850,24,
                R"(通信対戦を終了します)",
                R"(結束「連線對戰」)",
                R"(end Online Battle)"
                ),
    TRANSLATE(0x1C0868,20,
                R"(前の画面に戻ります)",
                R"( )",
                R"( )"
                ),
//    TRANSLATE(0x1C087C,8,
//              R"(Ｂ−１)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0884,8,
//              R"(Ｂ−２)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C088C,8,
//              R"(Ｃ−１)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0894,8,
//              R"(Ｃ−２)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C089C,8,
//              R"(Ｄ−１)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C08A4,8,
//              R"(Ｄ−２)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C08AC,24,
                R"(Ｌトリガー ：ページ送り)",
                R"(Ｌ掣     ：上一頁)",
                R"(L: Previous Page)"
                ),
    TRANSLATE(0x1C08C4,24,
                R"(Ｒトリガー ：ページ送り)",
                R"(Ｒ掣     ：下一頁)",
                R"(R: Next Page)"
                ),
//    TRANSLATE(0x1C08DC,20,
//              R"(Ｃボタン   ：決定)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C0908,32,
                R"(知らない人と対戦や会話をします)",
                R"( 請不要超越現實世界的道徳標準)",
                R"(You are playing with strangers)"
                ),
    TRANSLATE(0x1C0928,40,
                R"(マナーやエチケットに気をつけましょう)",
                R"( 使他人感到冒犯或煩擾、注意網絡禮儀)",
                R"( Be mindful of manners and etiquette)"
                ),
//    TRANSLATE(0x1C0950,36,
//              R"(不特定多数の人が会話を見ています)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0974,36,
//              R"(住所や電話番号を教えてはいけません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0998,24,
//              R"(公序良俗に反する発言)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C09B0,36,
//              R"(法律に触れる発言をしてはいけません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C09D4,20,
//              R"(対戦中は１分１３円)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C09E8,32,
//              R"(それ以外は３分１０円の課金です)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0A08,36,
//              R"(通信対戦中に本体のふたを開けたり)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0A2C,32,
//              R"(電源を切ったりしてはいけません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0A4C,24,
//              R"(電話料金がかかります)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0A64,28,
//              R"(つなぎ過ぎに注意しましょう)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C0A80,16,
                R"(　ＩＤ選択　)",
                R"(　ＩＤ選擇　)",
                R"(ID Selection)"
                ),
    TRANSLATE(0x1C0A90,16,
                R"(相手のＩＤへ)",
                R"(選擇)",
                R"(Select)"
                ),
    TRANSLATE(0x1C0AA0,8,
                R"(削除)",
                R"(刪除)",
                R"(Delete)"
                ),
    TRANSLATE(0x1C0AA8,16,
                R"(ヒマラヤ山脈)",
                R"(喜馬拉雅山脈)",
                R"(Himalayans)"
                ),
    TRANSLATE(0x1C0AB8,20,
                R"(タクラマカン砂漠)",
                R"(塔克拉瑪干沙漠)",
                R"(Taklamakan Desert)"
                ),
    TRANSLATE(0x1C0ACC,24,
                R"(ククルス・ドアンの島)",
                R"(庫克羅斯・徳安之島)",
                R"(Cucuruz Doan's Island)"
                ),
    TRANSLATE(0x1C0AE4,20,
                R"(黒海南岸森林地帯)",
                R"(黒海南岸森林地帶)",
                R"(Black Sea Forest)"
                ),
    TRANSLATE(0x1C0AF8,12,
                R"(オデッサ)",
                R"(奧迪沙)",
                R"(Odessa)"
                ),
    TRANSLATE(0x1C0B04,16,
                R"(ベルファスト)",
                R"(貝爾法斯特)",
                R"(Belfast)"
                ),
    TRANSLATE(0x1C0B14,12,
                R"(シアトル)",
                R"(西雅圖)",
                R"(Seattle)"
                ),
    TRANSLATE(0x1C0B20,8,
                R"(大西洋)",
                R"(大西洋)",
                R"(Atlanti)"
                ),
    TRANSLATE(0x1C0B28,16,
                R"(ニューヤーク)",
                R"(紐約)",
                R"(New York)"
                ),
    TRANSLATE(0x1C0B38,20,
                R"(グレートキャニオン)",
                R"(大峽谷)",
                R"(Grand Canyon)"
                ),
    TRANSLATE(0x1C0B4C,12,
                R"(ジャブロー)",
                R"(査布羅)",
                R"(Jaburo)"
                ),
    TRANSLATE(0x1C0B58,12,
                R"(地下基地)",
                R"(地下基地)",
                R"(UG Complex)"
                ),
    TRANSLATE(0x1C0B64,12,
                R"(ソロモン)",
                R"(所羅門)",
                R"(Solomon)"
                ),
    TRANSLATE(0x1C0B70,16,
                R"(ソロモン宙域)",
                R"(所羅門宙域)",
                R"(Solomon (Space))"
                ),
    TRANSLATE(0x1C0B80,24,
                R"(ア・バオア・クー宙域)",
                R"(阿・巴瓦・庫 宙域)",
                R"(A Baoa Qu (Space))"
                ),
    TRANSLATE(0x1C0B98,24,
                R"(ア・バオア・クー外部)",
                R"(阿・巴瓦・庫 外部)",
                R"(A Baoa Qu (Outter))"
                ),
    TRANSLATE(0x1C0BB0,24,
                R"(ア・バオア・クー内部)",
                R"(阿・巴瓦・庫 内部)",
                R"(A Baoa Qu (Inner))"
                ),
    TRANSLATE(0x1C0BC8,20,
                R"(テキサスコロニー)",
                R"(徳薩斯州殖民地)",
                R"(Texas Colony)"
                ),
    TRANSLATE(0x1C0BDC,12,
                R"(衛星軌道１)",
                R"(衛星軌道１)",
                R"(Sat.Orbit 1)"
                ),
    TRANSLATE(0x1C0BE8,12,
                R"(衛星軌道２)",
                R"(衛星軌道２)",
                R"(Sat.Orbit 2)"
                ),
    TRANSLATE(0x1C0BF4,16,
                R"(サイド６宙域)",
                R"(SIDE 6 宙域)",
                R"(SIDE 6 (Space))"
                ),
    TRANSLATE(0x1C0C04,16,
                R"(サイド７内部)",
                R"(SIDE 7 内部)",
                R"(SIDE 7 (Inner))"
                ),
    TRANSLATE(0x1C0C14,12,
                R"(ルナツー)",
                R"(Luna II)",
                R"(Luna II)"
                ),
//    TRANSLATE(0x1C0C20,4,
//              R"(　)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0C24,4,
//              R"(■)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0C28,116,
//              R"(ろー　　ちこそしいはきくにまのりもみらせたすとかなひてさんつぬふあうえおやゆよわ　　　　　ほへ゛゜　むれけ　ねるめ)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C0C9C,116,
//              R"(ロー　　チコソシイハキクニマノリモミラセタストカナヒテサンツヌフアウエオヤユヨワ　　　　　ホヘ゛゜　ムレケ　ネルメ)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C2414,16,
                R"(アムロ・レイ)",
                R"(阿寶・尼爾)",
                R"(Amuro)"
                ),
    TRANSLATE(0x1C2424,16,
                R"(カイ・シデン)",
                R"(凱・西丁)",
                R"(Kai)"
                ),
    TRANSLATE(0x1C2434,20,
                R"(ハヤト・コバヤシ)",
                R"(小林・隼人)",
                R"(Hayato)"
                ),
    TRANSLATE(0x1C2448,16,
                R"(リュウ・ホセイ)",
                R"(龍・荷西)",
                R"(Ryu)"
                ),
    TRANSLATE(0x1C2458,16,
                R"(セイラ・マス)",
                R"(馬茜・瑪斯)",
                R"(Sayla)"
                ),
    TRANSLATE(0x1C2468,20,
                R"(スレッガー・ロウ)",
                R"(史立加・羅奧)",
                R"(Sleggar)"
                ),
    TRANSLATE(0x1C247C,20,
                R"(赤い彗星のシャア)",
                R"(紅彗星馬沙)",
                R"(Char)"
                ),
    TRANSLATE(0x1C2490,16,
                R"(ランバ・ラル)",
                R"(頼巴・拉路)",
                R"(Ramba)"
                ),
    TRANSLATE(0x1C24A0,8,
                R"(ガイア)",
                R"(佳亞)",
                R"(Gaia)"
                ),
    TRANSLATE(0x1C24A8,12,
                R"(マッシュ)",
                R"(馬殊)",
                R"(Mash)"
                ),
    TRANSLATE(0x1C24B4,12,
                R"(オルテガ)",
                R"(奧迪加)",
                R"(Ortega)"
                ),
    TRANSLATE(0x1C24C0,12,
                R"(マ・クベ)",
                R"(馬・古比)",
                R"(M'qube)"
                ),
    TRANSLATE(0x1C24CC,16,
                R"(ララァ・スン)",
                R"(娜娜・絲)",
                R"(Lalah)"
                ),
    TRANSLATE(0x1C24DC,16,
                R"(ドズル・ザビ)",
                R"(多茲爾・薩比)",
                R"(Dozle)"
                ),
    TRANSLATE(0x1C24EC,16,
                R"(ガルマ・ザビ)",
                R"(加曼・薩比)",
                R"(Garma)"
                ),
//    TRANSLATE(0x1C24FC,12,
//              R"(0123456789-)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C2508,32,
//              R"(  このパイロット名は使えません)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C2528,36,
//              R"( パイロット名を入力してください )",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C254C,4,
//              R"( )",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C2654,24,
//              R"(Ａボタンを押して下さい)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C266C,8,
//              R"(******)",
//              R"( )",
//              R"( )"
//              ),
//
//
    TRANSLATE(0x1C2D2C,8,
                R"(未登録)",
                R"(未註冊)",
                R"(Empty)"
                ),
    TRANSLATE(0x1C2D58,8,
                R"(昇　格)",
                R"(升　格)",
                R"(Promote)"
                ),
    TRANSLATE(0x1C2D60,8,
                R"(降　格)",
                R"(降　格)",
                R"(Demote)"
                ),
    TRANSLATE(0x1C2D6C,8,
                R"(二等兵)",
                R"(二等兵)",
                R"(PSC)"
                ),
    TRANSLATE(0x1C2D74,8,
                R"(一等兵)",
                R"(一等兵)",
                R"(PFC)"
                ),
    TRANSLATE(0x1C2D7C,8,
                R"(上等兵)",
                R"(上等兵)",
                R"(L/CPL)"
                ),
    TRANSLATE(0x1C2D84,8,
                R"(伍　長)",
                R"(伍　長)",
                R"(CPL)"
                ),
    TRANSLATE(0x1C2D8C,8,
                R"(軍　曹)",
                R"(軍　曹)",
                R"(SGT)"
                ),
    TRANSLATE(0x1C2D94,8,
                R"(曹　長)",
                R"(曹　長)",
                R"(SGM)"
                ),
    TRANSLATE(0x1C2D9C,8,
                R"(少　尉)",
                R"(少　尉)",
                R"(2.LT)"
                ),
    TRANSLATE(0x1C2DA4,8,
                R"(中　尉)",
                R"(中　尉)",
                R"(1.LT)"
                ),
    TRANSLATE(0x1C2DAC,8,
                R"(大　尉)",
                R"(大　尉)",
                R"(CAP)"
                ),
    TRANSLATE(0x1C2DB4,8,
                R"(少　佐)",
                R"(少　佐)",
                R"(MAJ)"
                ),
    TRANSLATE(0x1C2DBC,8,
                R"(中　佐)",
                R"(中　佐)",
                R"(L/COL)"
                ),
    TRANSLATE(0x1C2DC4,8,
                R"(大　佐)",
                R"(大　佐)",
                R"(COL)"
                ),
    TRANSLATE(0x1C2DCC,8,
                R"(少　将)",
                R"(少　將)",
                R"(M/GEN)"
                ),
    TRANSLATE(0x1C2DD4,8,
                R"(中　将)",
                R"(中　將)",
                R"(L/GEN)"
                ),
    TRANSLATE(0x1C2DDC,8,
                R"(大　将)",
                R"(大　將)",
                R"(GEN)"
                ),
    TRANSLATE(0x1C2DE4,18,
                R"(%8u位／%8u人中)",
                R"(%8u位／%8u人中)",
                R"(%8u／%8u total)"
                ),
//    TRANSLATE(0x1C2E68,16,
//              R"(%4d:%02d:%02d)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C2E78,8,
//              R"(× %d)",
//              R"( )",
//              R"( )"
//              ),
//
//
//
//
    TRANSLATE(0x1C2E80,8,
                R"(%8u円)",
                R"(%8u円)",
                R"(¥%8u)"
                ),
    TRANSLATE(0x1C2E88,16,
                R"(課金予測情報)",
                R"(預計課金金額)",
                R"(Est Service Fee)"
                ),
//    CUSTOMIZE(0x1C2E88,16,
//              R"(課金予測情報)",
//              R"(對戰中人數)",
//              R"(In battle)",
//              R"(対戦中人数)"
//              ),
//
//
    TRANSLATE(0x1C2E98,32,
                R"(更新情報、メンテナンス情報等の)",
                R"(査閲最新情報、server維護等資料)",
                R"(Various info such as updates,)"
                ),
    TRANSLATE(0x1C2EB8,32,
                R"(様々な情報を見ることができます)",
                R"( )",
                R"(maintenance, etc. can be viewed)"
                ),
    TRANSLATE(0x1C2ED8,40,
                R"(通信ゲーム利用規約を見ることができます)",
                R"(査閲連線對戰的條款細則)",
                R"(Read the Terms and Condition)"
                ),
    TRANSLATE(0x1C2F04,44,
                R"(各モードのランキングを見ることができます)",
                R"(査閲排名榜和自己的排名)",
                R"(Check the battle ranking and your ranking)"
                ),
//    TRANSLATE(0x1C2F30,8,
//              R"(ご案内)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C2F38,4,
//              R"(%3d)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C2F3C,4,
                R"(人)",
                R"(人)",
                R"()" //JIS F040 = person symbol (c0ed65bf.png)
                ),
//    TRANSLATE(0x1C2F40,4,
//              R"(%2d)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C2F44,20,
                R"(Ｇ 　人／ＤＸ 　人)",
                R"(Ｇ 　人／ＤＸ 　人)",
                R"(Ｇ 　／ＤＸ 　)" //JIS F040 = person symbol (c0ed65bf.png)
                ),
    TRANSLATE(0x1C2F58,48,
                R"(チャットログ表示中／Ｘボタン・Ｂボタンで解除)",
                R"(正在瀏覽聊天紀録：按Ｘ／Ｂ掣解除)",
                R"(Showing chat log: Press X / B to cancel)"
                ),
//    TRANSLATE(0x1C2F88,8,
//              R"(%2d：)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C2F90,28,
                R"(−−−−募集なし−−−−)",
                R"(−−−−未有招募−−−−)",
                R"(−−No recruitment−−)"
                ),
//    TRANSLATE(0x1C2FAC,4,
//              R"(▲)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C2FB0,4,
//              R"(▼)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C2FB4,8,
//              R"(　Ｘ　)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C2FBC,8,
                R"(募集中)",
                R"(招募中)",
                R"(Recruit)"
                ),
    TRANSLATE(0x1C2FC4,8,
                R"(準備中)",
                R"(準備中)",
                R"(Standby)"
                ),
    TRANSLATE(0x1C2FCC,8,
                R"(出撃中)",
                R"(出撃中)",
                R"(Sortie)"
                ),
    TRANSLATE(0x1C2FD4,36,
                R"(パートナーを自動で選んでくれます)",
                R"(系統將會自動配對Partner)",
                R"(Auto-match a partner for you)"
                ),
    TRANSLATE(0x1C2FF8,44,
                R"(募集している人の中からパートナーを選びます)",
                R"(在招募中的人當中選擇Partner)",
                R"(Check who is recruiting)"
                ),
//    TRANSLATE(0x1C3024,4,
//              R"(　)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C3028,36,
                R"(一緒に戦うパートナーを募集します)",
                R"(招募Partner)",
                R"(Recruit for partners)"
                ),
    TRANSLATE(0x1C306C,28,
                R"(パートナー募集待機中です)",
                R"(正在等候Partner加入)",
                R"(Waiting for partner)"
                ),
    TRANSLATE(0x1C3088,16,
                R"(出撃準備中です)",
                R"(出撃準備中)",
                R"(Prepare sortie)"
                ),
    TRANSLATE(0x1C3098,44,
                R"(チャットログ表示中／Ｘボタン・Ｂボタンで解除)",
                R"(正在瀏覽聊天紀録：按Ｘ／Ｂ掣解除)",
                R"(Showing chat log: Press X / B to cancel)"
                ),
    TRANSLATE(0x1C3174,10,
                R"(%6d戦)",
                R"(%6d戰)",
                R"(%6dBattle)"
                ),
    TRANSLATE(0x1C31AC,8,
                R"(検索中)",
                R"(搜尋中)",
                R"(Finding)"
                ),
    TRANSLATE(0x1C31B4,8,
                R"(選択中)",
                R"(選擇中)",
                R"(Select)"
                ),
    TRANSLATE(0x1C31BC,16,
                R"(%4d戦%4d勝%4d敗)",
                R"(%4d戰%4d勝%4d敗)",
                R"(%4dB%4dW%4dL)"
                ),
    CUSTOMIZE(0x1C31CC,44,
                R"(これより先は１分１３円の課金がかかります)",
                R"(由而家起直到番去大廳,千祈唔好閂game斷線)",
                R"(DON'T DISCONNECT until you're back to lobby)",
                R"(ロビーに戻るまで接続を解除しないでください)"
                ),
//    TRANSLATE(0x1C31F8,6,
//              R"(%2d)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C3218,16,
                R"(戦　友　登　録)",
                R"(戰　友　加　入)",
                R"( Add Friend)"
                ),
    // TRANSLATE(0x1C3228,8,
    //             R"(ＩＤ)",
    //             R"( )",
    //             R"( )"
    //             ),
    TRANSLATE(0x1C3230,8,
                R"(ＨＮ)",
                R"(網名)",
                R"(ＨＮ)"
                ),
    TRANSLATE(0x1C3238,8,
                R"(登録)",
                R"(加戰友)",
                R"(Add)"
                ),
    TRANSLATE(0x1C3240,48,
                R"(方向ボタン左右：登録切り替え／Ｂボタン：終了)",
                R"(左右方向掣：切換加入戰友、Ｂ掣：完成)",
                R"(Left/Right：Add friend、B：Confirm)"
                ),
    TRANSLATE(0x1C3270,24,
                R"(あと　　人登録できます)",
                R"(可加　　人為戰友)",
                R"(　　　　slots available)"
                ),
    TRANSLATE(0x1C3288,28,
                R"(戦友ファイルがいっぱいです)",
                R"(       戰友名單已滿)",
                R"(   Friend list is full)"
                ),
    TRANSLATE(0x1C32A4,8,
                R"(しない)",
                R"(不加入)",
                R"(No)"
                ),
    TRANSLATE(0x1C32AC,8,
                R"(す　る)",
                R"(加　入)",
                R"(Yes)"
                ),
    TRANSLATE(0x1C32B4,8,
                R"(登録済)",
                R"(已加入)",
                R"(Added)"
                ),
//    TRANSLATE(0x1C32BC,8,
//              R"(%10u)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C32C4,8,
                R"(位／)",
                R"(位／)",
                R"(／)"
                ),
    TRANSLATE(0x1C32CC,8,
                R"(人中)",
                R"(人中)",
                R"( total)"
                ),
//    TRANSLATE(0x1C32D4,4,
//              R"(%5d)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C32D8,12,
//              R"(９９９９９)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C32E4,4,
                R"(戦)",
                R"(戰)",
                R"(B)"
                ),
    TRANSLATE(0x1C32E8,4,
                R"(勝)",
                R"(勝)",
                R"(W)"
                ),
    TRANSLATE(0x1C32EC,4,
                R"(敗)",
                R"(敗)",
                R"(L)"
                ),
    TRANSLATE(0x1C33A4,16,
                R"(%6d位／%6d人中)",
                R"(%6d位／%6d人中)",
                R"(%6d／%6d total)"
                ),
    TRANSLATE(0x1C33B4,8,
                R"(%8d円)",
                R"(%8d円)",
                R"(¥%8d)"
                ),
    TRANSLATE(0x1C33BC,8,
                R"(%4d人)",
                R"(%4d人)",
                R"(%4d)" //JIS F040 = person symbol (c0ed65bf.png)
                ),
//    TRANSLATE(0x1C3432,18,
//              R"(砲ランキング選択)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C3444,8,
//              R"(%10u)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C344C,8,
                R"(位／)",
                R"(位／)",
                R"(／)"
                ),
    TRANSLATE(0x1C3454,8,
                R"(人中)",
                R"(人中)",
                R"( total)"
                ),
//    TRANSLATE(0x1C345C,4,
//              R"(%5d)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C3460,8,
//              R"(99999)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C3468,4,
                R"(戦)",
                R"(戰)",
                R"(B)"
                ),
    TRANSLATE(0x1C346C,4,
                R"(勝)",
                R"(勝)",
                R"(W)"
                ),
    TRANSLATE(0x1C3470,4,
                R"(敗)",
                R"(敗)",
                R"(L)"
                ),
    TRANSLATE(0x1C3474,8,
                R"(無効)",
                R"(無效)",
                R"(Invalid)"
                ),
//    TRANSLATE(0x1C348C,8,
//              R"(　Ｘ　)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C3494,8,
                R"(%4d人)",
                R"(%4d人)",
                R"(%4d)" //JIS F040 = person symbol (c0ed65bf.png)
                ),
    TRANSLATE(0x1C349C,24,
                R"(更新情報やランキング)",
                R"(最新情報和排名榜)",
                R"(Updates and Rankings)"
                ),
    TRANSLATE(0x1C34B4,32,
                R"(利用規約などの情報が見れます)",
                R"(亦可査閲條款細則或其他資料)",
                R"(Check T&C and other info also)"
                ),
    TRANSLATE(0x1C34D4,32,
                R"(カーソルを戦場に合わせて下さい)",
                R"(請移動cursor選擇戰場)",
                R"(Select battlefield using cursor)"
                ),
    TRANSLATE(0x1C34F4,8,
                R"(%3d人)",
                R"(%3d)",
                R"(%3d)" //JIS F040 = person symbol (c0ed65bf.png)
                ),
    TRANSLATE(0x1C3504,12,
                R"(メール一覧)",
                R"(査閲信息)",
                R"(Mail List)"
                ),
    TRANSLATE(0x1C3510,20,
                R"(メールはありません)",
                R"( )",
                R"( )"
                ),
//    TRANSLATE(0x1C3524,8,
//              R"(ＩＤ)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C352C,8,
                R"(ＨＮ)",
                R"(網名)",
                R"(ＨＮ)"
                ),
    TRANSLATE(0x1C3534,8,
                R"(状態)",
                R"(状態)",
                R"(Status)"
                ),
    TRANSLATE(0x1C3544,8,
                R"(未読)",
                R"(未讀)",
                R"(Unread)"
                ),
    TRANSLATE(0x1C354C,8,
                R"(%4d人)",
                R"(%4d人)",
                R"(%4d)" //JIS F040 = person symbol (c0ed65bf.png)
                ),
    TRANSLATE(0x1C356C,12,
                R"(発言者一覧)",
                R"(査閲發言者)",
                R"(Speakers)"
                ),
    TRANSLATE(0x1C3578,20,
                R"(発言者はいません)",
                R"(沒有任何人發言)",
                R"(No speaker)"
                ),
    TRANSLATE(0x1C358C,16,
                R"(発言者　%2d人)",
                R"(發言者　%2d人)",
                R"(Speaker %2d)" //JIS F040 = person symbol (c0ed65bf.png)
                ),
//    TRANSLATE(0x1C359C,8,
//              R"(ＩＤ)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C35A4,8,
                R"(ＨＮ)",
                R"(網名)",
                R"(ＨＮ)"
                ),
    TRANSLATE(0x1C35AC,8,
                R"(登録)",
                R"(加戰友)",
                R"(Add)"
                ),
    TRANSLATE(0x1C35B4,48,
                R"(方向ボタン左右：登録切り替え／Ｂボタン：終了)",
                R"(左右方向掣：切換加入戰友、Ｂ掣：完成)",
                R"(Left/Right：Add friend、B：Confirm)"
                ),
    TRANSLATE(0x1C35E4,24,
                R"(あと　　人登録できます)",
                R"(可加　　人為戰友)",
                R"(　　　　slots available)"
                ),
//    TRANSLATE(0x1C35FC,4,
//              R"(%2d)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C3600,28,
                R"(戦友ファイルがいっぱいです)",
                R"(戰友儲存空間已滿)",
                R"(Friend list is full)"
                ),
//    TRANSLATE(0x1C361C,4,
//              R"(▲)",
//              R"( )",
//              R"( )"
//              ),
//    TRANSLATE(0x1C3620,4,
//              R"(▼)",
//              R"( )",
//              R"( )"
//              ),
    TRANSLATE(0x1C3624,8,
                R"(しない)",
                R"(不加入)",
                R"(No)"
                ),
    TRANSLATE(0x1C362C,8,
                R"(す　る)",
                R"(加　入)",
                R"(Yes)"
                ),
    TRANSLATE(0x1C3634,8,
                R"(登録済)",
                R"(已加入)",
                R"(Added)"
                ),
//    TRANSLATE(0x1C363C,8,
//              R"(HN変更)",
//              R"( )",
//              R"( )"
//              ),
//
//
    TRANSLATE(0x1C38C4,32,
                R"(メモリーカードチェック中・・・)",
                R"(正在檢査記憶體・・・)",
                R"(Checking memory card...)"
                ),
    TRANSLATE(0x1C38E4,36,
                R"(通信ゲームファイルをロードします)",
                R"(載入連線遊戲存档)",
                R"(Load the online game save)"
                ),
    TRANSLATE(0x1C3908,32,
                R"(ファイルをロードしない場合は)",
                R"(如不載入，連線相關的資料)",
                R"(If you do not load the file,)"
                ),
    TRANSLATE(0x1C3928,40,
                R"(モデムの設定や他のプレイヤーのＩＤ等が)",
                R"(例如modem設定、其他玩家的ID等)",
                R"(modem settings, other players' IDs,)"
                ),
    TRANSLATE(0x1C3950,36,
                R"(セーブできませんのでご注意ください)",
                R"(將不會被儲存。)",
                R"(etc. will not be saved.)"
                ),
    TRANSLATE(0x1C3974,40,
                R"(通信ゲームファイルをロードしますか？)",
                R"(是否載入？)",
                R"(Do you want to load the file?)"
                ),
    TRANSLATE(0x1C399C,44,
                R"(通信ゲームファイルが見つかりませんでした)",
                R"(找不到連線遊戲存档)",
                R"(Online game save not found)"
                ),
    TRANSLATE(0x1C39C8,28,
                R"(ファイルを作成しない場合は)",
                R"(如不建立，連線相關的資料)",
                R"(If you do not create it,)"
                ),
    TRANSLATE(0x1C39E4,20,
                R"(新規作成しますか？)",
                R"(是否建立？)",
                R"(Create the file?)"
                ),
    TRANSLATE(0x1C39F8,20,
                R"(「通信対戦」では)",
                R"(參與「連線対戦」)",
                R"(For "Online Battle")"
                ),
    TRANSLATE(0x1C3A0C,36,
                R"(パイロットネームの入力が必要です)",
                R"(必須輸入駕駛員名稱)",
                R"(Pilot name must be entered)"
                ),
    TRANSLATE(0x1C3A30,40,
                R"(パイロットネームの入力をお願いします)",
                R"(遊戲進行時其他玩家會看到駕駛員名稱)",
                R"(Other players will see it during battle)"
                ),
    TRANSLATE(0x1C3A58,40,
                R"(メモリーカードが見つかりませんでした)",
                R"(找不到記憶體(Sega VMU))",
                R"(Cannot find memory card (Sega VMU))"
                ),
    TRANSLATE(0x1C3A80,48,
                R"(通信ゲームファイルを利用せずに先へ進みますか？)",
                R"(不儲存連線遊戲資料，繼續進行遊戲？)",
                R"(Continue online game without online game save?)"
                ),
    TRANSLATE(0x1C3AB0,36,
                R"(通信対戦に入ると、セーブしていない)",
                R"(連線対戦的資料不會被儲存)",
                R"(Online battle data is not saved)"
                ),
    TRANSLATE(0x1C3AD4,32,
                R"(データが失われることがあります)",
                R"(所有相關資料將會遺失。)",
                R"(All relevant data wil be lost.)"
                ),
    TRANSLATE(0x1C3AF4,32,
                R"(パイロットネームも保存します)",
                R"(要儲存駕駛員名稱等相關資料，)",
                R"(To save pilot name, do you want)"
                ),
    TRANSLATE(0x1C3B14,20,
                R"(セーブしますか？)",
                R"(建立連線遊戲存档？)",
                R"(to create gamesave?)"
                ),
    TRANSLATE(0x1C3B28,8,
                R"(はい)",
                R"(確定)",
                R"(Yes)"
                ),
    TRANSLATE(0x1C3B30,12,
                R"(／いいえ)",
                R"(／取消)",
                R"(／No)"
                ),
    TRANSLATE(0x1C3B3C,8,
                R"(はい／)",
                R"(確定／)",
                R"(Yes／)"
                ),
    TRANSLATE(0x1C3B44,8,
                R"(いいえ)",
                R"(取消)",
                R"(No)"
                ),
    
    //    TRANSLATE(0x000000,99,
    //              R"(ス)",
    //              R"( )",
    //              R"( )"
    //              ),


};

for (Translation translation : translations)
{
     const char * text = translation.Text();
//    const char * text = translation.cantonese;
    if (text)
    {
        for (int i = 0; i < strlen(text); ++i)
        {
            WriteMem8_nommu(offset + translation.offset + i, u8(text[i]));
        }
        WriteMem8_nommu(offset + translation.offset + (u32)strlen(text), u8(0));
    }
}
