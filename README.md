# 網路系統程式開發與安全 期中專案：以 fork() 實作的並行 Client-Server 系統

## 資料夾結構
| 路徑 | 說明 |
|---|---|
| `Makefile` | 建置規則 |
| `src/` | 原始碼：`server.c`、`client.c`、`util.c`、`util.h` |
| `scripts/` | `test_concurrent.sh`（12 個 client 並行測試）、`experiments.sh`（三個異常對照實驗） |

## 使用
```bash
make                                # 編譯到 build/
./build/server 9000                 # 啟動 server，Ctrl+C 停止
./build/client 127.0.0.1 9000 INFO  # 另一個終端機：查詢系統資訊
make test                           # 並行測試
./scripts/experiments.sh            # 三個對照實驗
```
Log 等級：執行時 `LOG_LEVEL=0 ./build/server 9000`（0 DEBUG、1 INFO、2 WARN、3 ERROR）。
編譯時 `make CFLAGS='-Wall -Wextra -g -fPIC -DLOG_COMPILE_LEVEL=1'`。

## Server 參數
| 參數 | 作用 | 預設 |
|---|---|---|
| `port` | 監聽的 port（1 到 65535） | 9000 |
| `--timeout 秒數` | 連線這麼久沒送資料就關閉 | 30 |
| `--max-clients 數量` | 同時服務的 client 上限，超過回覆 `ERR server busy` | 64 |
| `--no-robust` | 關閉全部保護機制，供對照實驗使用 | 未啟用 |

## 保護機制與對照實驗
`experiments.sh` 會對機制 1 到 3 各跑一次「關閉保護／啟用保護」的對照。機制 4、5 server 也有實作，但腳本沒有對應的實驗。

| # | 異常狀況 | 機制 |
|---|---|---|
| 1 | 子行程結束沒被回收（zombie） | `SIGCHLD` handler + `waitpid` |
| 2 | Client 中途斷線 | 忽略 `SIGPIPE`，檢查 `send` 回傳值 |
| 3 | Server 快速重啟 | `SO_REUSEADDR` |
| 4 | Client 連線後不送資料 | `SO_RCVTIMEO` 閒置逾時 |
| 5 | 連線數過多 | 父行程計數，超過上限就拒絕 |
