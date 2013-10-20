[LangOptions]
LanguageName=Japanese
LanguageID=$0411
LanguageCodePage=0

[CustomMessages]

MyAppName=PuTTYごった煮版
MyAppVerName=PuTTY 0.60 ごった煮版
MyVersionDescription=PuTTYごった煮版セットアップ
MyVersionInfo=0.60 ごった煮版
PuTTYShortcutName=PuTTY
PuTTYComment=SSH, Telnet, Rlogin クライアント
PuTTYManualShortcutName=PuTTY マニュアル
PuTTYWebSiteShortcutName=PuTTY ウェブサイト
PSFTPShortcutName=PSFTP
PSFTPComment=コマンドライン 対話的 SFTP クライアント
PuTTYgenShortcutName=PuTTYgen
PuTTYgenComment=PuTTY SSH 鍵生成ユーティリティ
PageantShortcutName=Pageant
PageantComment=PuTTY SSH 認証エージェント
AdditionalIconsGroupDescription=追加のアイコン:
desktopiconDescription=デスクトップにPuTTYのアイコンを作成
desktopicon_commonDescription=全てのユーザー
desktopicon_userDescription=現在のユーザーのみ
quicklaunchiconDescription=クィックランチにPuTTYのアイコンを追加 (現在のユーザーのみ)
OtherTasksGroupDescription=その他のタスク:
associateDescription=.PPKファイル(PuTTY秘密鍵)をPageantとPuTTYgenに関連づける
PPKTypeDescription=PuTTY秘密鍵ファイル
PPKEditLabel=編集(&E)
UninstallStatusMsg=保存したセッション他を削除しています (オプショナル)...
SaveiniDescription=設定をINIファイルに保存する
PfwdComponentDescription=pfwd(ポートフォワード専用クライアント)
PlinkwComponentDescription=plinkw(コンソールウィンドウを表示しないPlink)
JpnLngComponentDescription=日本語メッセージファイル
SourceComponentDescription=ソースコード(パッチのみ)

[Messages]
; ユーザにコンピュータを再起動するかどうかを尋ねることができるので、
; 正確な理由を説明し、十分な情報に基づいた判断を下せるように
; 標準のメッセージを上書きするべきである。 (特に95%のユーザは、再起動を
; 必要としない、または欲していない; 誇張有り)
FinishedRestartLabel=一つ以上の [name] プログラムが実行中です。セットアッププログラムはこのコンピュータが再起動するまでこれらのプログラムファイルを置き換えることができません。今すぐ再起動しますか?
; このメッセージは/SILENTインストール時にメッセージボックスで表示される。
FinishedRestartMessage=一つ以上の [name] プログラムが実行中です。%nセットアッププログラムはこのコンピュータが再起動するまでこれらのプログラムファイルを置き換えることができません。%n%n今すぐ再起動しますか?
; それから、これはアンインストールしようとしたときに表示される。
UninstalledAndNeedsRestart=一つ以上の %1 プログラムが実行中です。%nこのプログラムファイルはこのコンピュータの再起動時に削除されます。%n%n今すぐ再起動しますか?
