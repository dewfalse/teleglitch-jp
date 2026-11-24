# Teleglitch: Die More Edition 日本語化MOD

Windows向けSteam版 v9.3（x86）用の非公式MODです。日本語描画にはWindowsのＭＳ ゴシックを使用します。

## プレイヤー向け

ビルド済みの配布ZIPを展開し、Steamの「管理」→「ローカルファイルを閲覧」でゲームのフォルダを開きます。ZIP内の `teleglitch-jp` を `Teleglitch.exe` の隣にコピーし、コピー先の `install.cmd` を実行してください。削除には `uninstall.cmd` を使います。詳細は [README-JA.txt](README-JA.txt) にあります。

## ソースからビルドする

1. Visual Studio 2022 または Build Tools の「C++ によるデスクトップ開発」（MSVC x86 ツールと Windows SDK）、PowerShell 7 を用意します。
2. **x86 Native Tools Command Prompt for VS 2022** を開き、チェックアウトしたリポジトリのルートに移動します。
3. `teleglitch-jp\build.cmd` を実行します。折り返し済みの物語辞書を再生成し、`teleglitch-jp\teleglitch_jp.dll` を作成します。64ビットWindowsでもビルド対象はx86です。

## テスト

正規に入手した対応版ゲームのコピーを用意し、その直下にこのリポジトリの `teleglitch-jp` をコピーします。x86 Native Tools Command Prompt でそのゲームコピーに移動し、`teleglitch-jp\tests\run.cmd` を実行してください。テスト用のデータはゲームのコピーから読み込みます。Steamのプロフィールやセーブは不要です。

## ファイル構成

| 場所 | 用途 |
| --- | --- |
| `teleglitch-jp/src/` | 日本語を描画するDLLのソース |
| `teleglitch-jp/*.lua` | ゲーム起動時の読み込み、翻訳の照合と行配置 |
| `teleglitch-jp/*.tsv`、`opening_ja.txt` | 翻訳辞書とOP本文 |
| `teleglitch-jp/tools/build_narratives.ps1` | 編集用の `narrative_ja.tsv` から表示用辞書を生成 |
| `teleglitch-jp/tests/` | Luaのゲーム定義との照合と描画テスト |
| `package.ps1` | ビルド済みDLLと配布対象ファイルを新しい出力先へコピー |

## 配布フォルダの作成

ビルド後、リポジトリのルートで PowerShell から `./package.ps1 -Destination ../release` を実行します。出力先には新しいフォルダを指定してください。そこにできた内容をまとめてZIPにするとプレイヤー向け配布物になります。

## ライセンス

MOD独自のコードとドキュメントには [MIT License](LICENSE.txt) を適用します。原作の文章と翻訳データなどの適用範囲は [NOTICE.txt](NOTICE.txt) に記載しています。
