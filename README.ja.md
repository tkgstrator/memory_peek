# memory_peek

[English](README.md)

`memory_peek` は、短時間だけ読み取り専用のデバッガをアタッチして、現在実行中のゲームプロセスのメモリを読み取る、実験的な専用 Atmosphère システムモジュールです。

TCP ポート 6000 で待機し、sys-botbase と互換性のあるシンプルなテキストコマンド方式（改行で区切られた 1 行 1 コマンド形式）を採用しています。NXCTF などの既存ツールからそのまま接続できますが、メモリ書き込み、コントローラー入力の偽装、プロセスのフリーズ、ファイルシステム操作といった余計な機能は一切含まず、メモリの読み取りに必要な最小限の命令のみを提供します。チートサービスおよびチート API は一切使用せず、書き込み・一時停止・制御系の呼び出しもありません。

**ステータス:** クロスコンパイルおよび静的監査（audit）のみ完了しています。実機での Switch 起動、Atmosphère 1.12.1 ランタイム環境、LAN 通信、実際のゲーム内メモリ読み取りは未テストです。対象の SD カードのインベントリ確認、仮の Program ID の衝突チェック、および直接のリカバリ手順の準備が完了するまで、実機へのデプロイを行わないでください。SD カード上に NSP ファイルを配置しただけでは、起動やゲームのメモリ読み取りができることの証明にはなりません。

ゲームを必要とするリクエストごとに、`pm:dmnt` からアプリケーションのプロセス ID を取得し（コマンド 4）、`svcDebugActiveProcess` でアタッチして `svcReadDebugProcessMemory` で読み取ります。Main モジュールのベースとビルド ID は `ldr:dmnt` の `GetProcessModuleInfo` から（sys-botbase と同様、モジュールがちょうど 2 つなら module[1]、それ以外は module[0]）、Title ID は `pm:info` の `GetProgramId` から、ヒープのベースは `svcGetInfo(HeapRegionAddress)` から取得します。デバッグハンドルはあらゆる失敗経路で、成功時もネットワーク送信や 16 進エンコードの前に `svcCloseHandle` で閉じられ、ソケット待ちの間にアタッチを保持することはありません。ただしデバッガのアタッチを検知するタイトルには影響する可能性があります。使用するのはアタッチ・読み取り・クローズのみで、書き込み・制御・クエリ・一時停止の呼び出しはありません。固定した devkitPro ツールチェーンでは JSON 入力を `force_debug_prod` true、`force_debug` false にすると、生成された NPDM の実効 `force_debug` フラグが hactool で true と表示されます。`allow_debug` は false のままです。プロセスは存在するがアタッチや各種取得に失敗した場合は `ERR READER_UNAVAILABLE` を返します。

## コマンド仕様・プロトコル

コマンドは、改行（LF または CRLF）で区切られた 1 行のプレーンテキスト（ASCII）として送信します。応答も同様に CRLF で区切られた 1 行のテキストとして返されます。プログラムからの利用はもちろん、ターミナルから `nc`（netcat）などで直接コマンドを手入力してテストすることも可能です。

サーバーは一度に 1 つのクライアント接続のみを処理し、無通信（アイドル）状態が 5 秒続くと自動的に切断します。`getVersion` はプロトコルバージョン `1.0.0` を返し、`getIdentity` はモジュール識別子 `memory_peek/1` を返します。

| コマンド | 応答 |
| --- | --- |
| `getTitleID` | 大文字の 16 進数 16 桁（Title ID） |
| `getMainNsoBase` | 大文字の 16 進数 16 桁（Main NSO ベースアドレス） |
| `getBuildID` | ビルド ID の先頭 8 バイト（大文字の 16 進数 16 桁） |
| `getHeapBase` | 大文字の 16 進数 16 桁（ヒープベースアドレス） |
| `getDiagnostics` | 読み取り専用プローブの結果を固定順の 16 進 `key=value` で並べた 1 行: `pm_rc`、`pid`、`attach_rc`、`ldr_rc`、`modules`、`info_rc`、`heap_rc`、`title`、`main`、`heap`（下記参照） |
| `isProgramRunning 0x<TITLE_ID>` | 指定したタイトルが現在実行中かどうかに応じて `1` または `0` |
| `peekMain 0x<OFFSET> 0x<LENGTH>` | Main NSO ベース + オフセットからの大文字 16 進数バイト列 |
| `peekAbsolute 0x<ADDRESS> 0x<LENGTH>` | 絶対仮想メモリ空間アドレスからの大文字 16 進数バイト列 |
| `peekAbsoluteMulti 0x<ADDRESS> 0x<LENGTH> ...` | 要求順に連結された大文字 16 進数バイト列 |

各行は最大 2,048 バイトに制限されており、メモリ読み取り要求は 1 リクエストあたり最大 32 範囲かつ合計 1 MiB までに制限されています。数値引数は 10 進数または `0x` プレフィックス付きの 16 進数を受け付けます。不正な形式や未定義のコマンドに対しては `ERR INVALID_REQUEST` を返します。対象ゲームが存在しない場合は `ERR NO_GAME`、読み取り失敗時は `ERR READ_FAILED`、読み取り中のゲーム切り替え検知時は `ERR GAME_CHANGED` をそれぞれ返します。実行中のアプリケーションをアタッチまたは調査できない場合は `ERR READER_UNAVAILABLE` を返します。

`getDiagnostics` はゲームが実行中でなくても使用できます。同じプローブを実行してデバッグハンドルを閉じ、生の 32 ビット結果（`pm_rc`、`attach_rc`、`ldr_rc`、`info_rc`、`heap_rc`。8 桁）と `pid`、`title`、`main`、`heap`（16 桁）、`modules`（個数）を返します。未到達または失敗した手順の値は 0 です。`pm_rc` が 0 かつ `pid` が 0 以外でない限りアタッチは行いません。

メモリ読み取りは、成功応答のバイト列を送信する前に全体がバッファリングされ、デバッグアタッチも解放されます。認証や暗号化の仕組みはありません。サービス稼働中は、同一ネットワークから接続可能なすべてのクライアントがゲームメモリの読み取りを要求できます。

## ビルドと検証

ホスト側の単体テストの実行には、C コンパイラと Python 3 が必要です:

```sh
make test
```

ピン留めされローカルにキャッシュされた devkitPro の Docker イメージを使用して、クロスコンパイルと監査を実施できます。これは静的ビルドで使用されているイメージダイジェストと同一です:

```sh
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/work -w /work \
  devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282 \
  make switch
```

ビルド成果物は `build/switch/memory_peek.elf`、`.nso`, `.npdm`、および `.nsp` です。現在のハッシュ値および検証結果は `BUILD-MANIFEST.md`（または `BUILD-MANIFEST.ja.md`）に記録されています。

`make switch` は `hactool` を使用して生成された NPDM 権限を抽出し、厳格なシステムコール（SVC）およびサービス接続のホワイトリストを照合し、ファイルシステム権限がゼロであることを確認し、禁止 API や未許可の SVC がないかソースコードと ELF シンボルをスキャンします。また、監査対象となった NPDM と NSO が NSP 内に含まれる正確な 2 つの構成要素であることを確認します。監査は、基本セットに加えて SVC `0x60`（DebugActiveProcess）と `0x6A`（ReadDebugProcessMemory）のみ、サービス `pm:dmnt`・`ldr:dmnt`・`pm:info`・`bsd:u` のみ、hactool 上の実効デバッグフラグ `force_debug` true / `allow_debug` false のみを許可し、書き込み・制御系のデバッグ SVC、その他のプロセス変更機能、追加のカーネル機能カテゴリ、チートサービス/API への参照、`pm:dmnt` のアプリケーション PID 取得以外のサービスディスパッチを拒否します。これらは静的検証であり、実機ランタイム環境での互換性や通信の秘匿性を保証するものではありません。

仮の Program ID は `0x43004D5045454B01` です。インストール前に対象の SD カード上で未使用であることを確認してください。本プロジェクトにはインストーラーは含まれておらず、SD カードの自動変更も行いません。実機のインベントリ確認、ロールバックの準備、実際の起動およびメモリ読み取りの検証は別個のデプロイメントゲートとなっており、`docs/superpowers/specs/2026-10-31-memory-peek-design.md` に記載されています。

## CIとリリース

[`integration.yaml`](.github/workflows/integration.yaml) は、`master` ブランチへのプッシュとプルリクエストで C・Python のフォーマットを検査します。プルリクエストでは commitlint でコミットメッセージも検査します。Switch 向けパッケージのビルドや公開は行いません。

[`deployment.yaml`](.github/workflows/deployment.yaml) は、`master` ブランチ上のコミットに対する `vX.Y.Z` タグのプッシュによってトリガーされます。ホストテスト、Switch 向けの新規クロスコンパイル、成果物の権限監査を実行し、転送された NSP のチェックサムを検証したうえで、NSP と `SHA256SUMS` を GitHub Releases に公開します。既存のリリースに対して同じタグを再実行した場合は、アセットを上書きせずにエラー終了します。リリースのバージョン番号は、通信プロトコルのバージョンを示す `getVersion` とは独立しています。このワークフローは GitHub へのビルド成果物の公開のみを行い、Switch へのインストールや上記の実機検証を代替するものではありません。

## 出典とライセンス

本リポジトリの実装は本プロジェクトのために書き下ろされたものであり、GPL-3.0-only の下でライセンスされています（`LICENSE` を参照）。コマンド体系と応答フォーマットは、既存の NXCTF 読み取りクライアントおよびプロトコル調査に使用した GPL-3.0 の sys-botbase 2.5 ソーススナップショットと照合されています。モジュール選択規則は sys-botbase に従い、`pm:dmnt` のアプリケーション PID コマンド ID は Atmosphère 1.12.1 と照合されています。sys-botbase や Atmosphère のソースコードファイルは本リポジトリにコピーされておらず、IPC 呼び出しは libnx に対して独自に実装されています。ビルドには devkitPro の libnx および devkitA64 を使用しており、それらのアップストリームのライセンスおよびサポート条件が適用されます。
