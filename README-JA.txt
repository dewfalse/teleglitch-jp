Teleglitch: Die More Edition 非公式日本語化MOD
Windows Steam版 v9.3（32ビットのゲーム本体）用

■ 導入
1. ゲームを終了し、配布ZIPを「すべて展開」します。
2. Steamライブラリでゲームを右クリックし、「管理」→「ローカルファイルを閲覧」
   を選びます。開いたフォルダに Teleglitch.exe があることを確認してください。
   別のドライブや追加のSteamライブラリにインストールした場合も同じ手順です。
3. ZIP内の teleglitch-jp フォルダを丸ごと、Teleglitch.exe と同じ階層にコピーします。
   フォルダの中身だけをゲーム直下に置くと動作しません。
4. コピー先の teleglitch-jp 内にある install.cmd をダブルクリックします。
   Installed または already installed と表示されたら完了です。
5. Steamから通常どおりゲームを起動します。ゲーム画面にフォーカスし、
   ロゴとオープニングの各画面でスペースキーを押してメニューへ進みます。
   「情報」→「物語」で発見済みの項目を開くと、日本語の見出しと本文を確認できます。

配置の目安
  ゲームフォルダ
    Teleglitch.exe
    lua
    teleglitch-jp
      install.cmd
      uninstall.cmd
      teleglitch_jp.dll
      翻訳辞書・その他のファイル

■ 更新
ゲームを終了し、配布版の teleglitch-jp を既存フォルダに上書きコピーして
install.cmd を実行してください。既存フォルダを先に削除すると復元用の
backup が失われます。text_settings.ini を調整した方は先に控えてください。

■ 削除
ゲームを終了し、ゲーム側の teleglitch-jp 内で uninstall.cmd を実行します。
元の menu.lua の復元に成功したことを確認してから、teleglitch-jp フォルダを
削除できます。次回起動から元の表示になります。セーブデータの操作は不要です。

■ 対応範囲
主なメニュー、オープニング、チュートリアル、ステージの記録・通知、
所持品と図鑑のアイテム説明、敵17種の説明本文、死亡時にランダムで表示される
メッセージ全58種類を日本語化します。
アイテム名、敵名・装甲表記、アリーナの一部などは英語です。
本文サイズの設定は text_settings.ini の body_scale です（初期値 0.70）。
変更は再起動後に反映されます。OP、所持品・図鑑の説明は個別のサイズ設定です。

■ 困ったとき
・Teleglitch.exe が見つからない場合は、ZIP内で実行していないか、また
  teleglitch-jp が二重に入れ子になっていないか確認してください。
・Unsupported Teleglitch.exe は対象ビルドとの不一致です。別ビルドは未対応です。
・menu.lua やバックアップの不一致で停止した場合は、他のMODや手編集を確認して
  ください。復元できるまで backup と installation.json は保管してください。
・日本語が出ない場合は teleglitch-jp 内の renderer.log と、Windowsで
  ＭＳ ゴシックが利用できるかを確認してください。
・オープニング中に×を押してもすぐ閉じない場合は、スペースキーを複数回押し、
  画面が進んでウィンドウが閉じたことを確認してください。

■ ライセンス
MOD独自のコードは MIT License です。原作と翻訳データの扱いは NOTICE.txt、
ライセンス全文は LICENSE.txt をご覧ください。
