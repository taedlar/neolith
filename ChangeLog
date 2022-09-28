Neolith-0.1.2 變動
==================

- 允許 master object 在 effective UID 為 0 的狀況下產生物件。

- 修正在某些狀況下 crash() 被呼叫, 但程式無法結束的錯誤情形。

- 改良 svalue_to_string() 印出字串值時, 將控制字元 (包括 TAB, CR, LF 等)
  以 LPC 的 escape sequence 表示, 便於偵錯時閱讀。

- 加強 svalue_to_string() 的縮排功能, 修正 array 與 mapping 的縮排格式。
  帶有縮排的輸出方式, 所有元素後面都會加上逗號 (包括最後一個元素) 及換列,
  方便編輯時複製; 不帶縮排的輸出方式最後一個元素的逗號會被拿掉, 以縮短字
  串長度。

- 改良 dump_trace() 的效率, 大幅減少產生錯誤報告時的記憶體配置次數。

- 啟動時若 config 檔指定了 DebugLogFile, 而該 log 檔無法被開啟與寫入, 會強制
  結束。

Neolith-0.1.1 變動
==================

- 修正若干在 FreeBSD 上編譯的問題，FreeBSD 使用者請關閉 NLS 功能 (configure
  --disable-bls)。

- [CRASHER FIXED] 修正當發生 Too deep recursion 或 Eval cost too large 時, 
  由於變數堆疊與控制堆疊可能已經 overflow, 因此不再呼叫如 (s)printf 的
  object_name 等 apply 而導致遞迴呼叫 mudlib error handler。 (fixed in
  MudOS v22.1pre3)

- 修正 flush_message 在使用者因為異常狀況切斷連線後, 不會繼續傳送資料給已經
  斷線的使用者, 這在某些 BSD 系統中會造成 process 被停止。

- [CRASHER FIXED] 修正 repeat_string 若產生的字串過大會造成 crash 的問題。
  (fixed in MudOS v22.2b12, MudOS 的修正方式是自動將過長的字串 truncate 掉,
  我認為產生過長字串應該是程式設計上有問題, 因此 neolith 的修正方式是中斷
  程式並丟出 runtime error)

- [CRASHER FIXED] 修正在 filter 一個 mapping 的 filter 函數中，對原 mapping
  執行 map_delete 會造成 crash 的問題。 (fixed in MudOS v22.2b8)

  測試方法:

  mapping m = ([ "a": 1, "b": 2, "c": 3 ]);

  m = filter (m, (: map_delete ($(m), $1) :));

- [CRASHER FIXED] 修正在 notify_fail fucntion 中 destruct this_player() 會
  造成 crash 的問題。

  測試方法:

  add_action ("do_crash", "crash");
  ...
  int
  do_crash (string arg)
  {
    seteuid (geteuid (this_player()));
    return notify_fail ((: destruct, this_player() :));
  }

- [CRASHER FIXED] 修正以下的 crash 問題: (fixed in MudOS v22.1b27)

  mapping m = (["a":1, "b":1]);

  m *= m;
