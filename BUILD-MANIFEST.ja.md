# ローカルビルドマニフェスト — 2026-11-07

[English](BUILD-MANIFEST.md)

このマニフェストは、現在のソースツリーから作成した監査済みパッケージの記録です。稼働中の Switch の SD に配置し、本体再起動後に読み込まれました。直前に配置したパッケージ（`cf255e1525c3d455171ed1fe2198a5c35bc9aec7370f9833afa864e56c084817`）は TCP 待受けまで起動しましたが、実行中のスプラ3へのアタッチで `0xFA01` を返しました。その NPDM は `ForceDebugProd`（`0x0004FFFF`）でした。新しいパッケージは、この権限を `ForceDebug`（`0x0008FFFF`）に変更します。

| 項目 | 値 |
| --- | --- |
| ツールチェーン | `devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282` |
| Program ID | 仮割り当て `0x43004D5045454B01` |
| TCP ポート / 識別子 | `6000` / `memory_peek/1` |
| NSP SHA-256 | `1b383be72aaa3fda197a83cd1f2cf760033b89b326899c86a10d83e31931ed93` |
| NPDM SHA-256 | `4cbed322a3028ae336cc45706fadf3a4b82cb976863f725468d5031ca85178cf` |
| NSO SHA-256 | `18c5c1b23201bbe95403740c6b6a476bf71fb06a2c2633ef6c59b0642d7ae533` |

`make test` で C 言語のテスト 2 本と Python のテスト 24 件が成功しました。固定した Docker イメージで `make switch` を実行し、クロスコンパイルと `tools/audit.py` の監査が成功しました。監査で確認した内容は次のとおりです。

- NSP には監査対象の NSO と NPDM のみが含まれます。
- ACID と ACI0 の両方のカーネル権限領域に `ForceDebug`（`0x0008FFFF`）が一つずつあり、`ForceDebugProd` や複合デバッグ権限はありません。
- SVC とサービスの厳密な許可リスト、ファイルシステム権限ゼロ、ソースと ELF シンボルの制限が守られ、チート・書込み・デバッガ制御 API はありません。
- NSO の SHA-256 は直前の起動再試行版と同じで、変更されたのは NPDM の権限メタデータだけです。

稼働中 SD の `atmosphere/package3` の SHA-256 は、Atmosphère 1.12.1 の公式配布物と一致します。本体再起動後、稼働中 SD の NSP のハッシュがこのマニフェストと一致しました。実行中のスプラ3で `getDiagnostics` は `attach_rc=0`、Title ID `0100C2500FC20000`、ゼロでない Main とヒープのベースアドレスを返しました。`getBuildID` は `28C4287AEE36F749` を返し、`peekMain 0x0 0x10` で16バイトを読み取れました。読出し後もゲームを操作できるかどうかは、ユーザーの確認待ちです。切り戻し用に直前の NSP を保持します。このビルドではチートファイルを使用しません。
