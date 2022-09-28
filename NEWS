※這個檔案列出的僅限於「使用者可見」的變動，有關所有修訂的詳細內容請參考
  ChangeLog。

Neolith-0.1.2 變動摘要
======================

 - 當 mudlib 不定義 master::creator_file() 時, 允許產生物件, 並將 uid
   設定為 NONAME。

 - 組態設定新增以下項目：

	StackSize		執行 LPC 程式的堆疊大小
	MaxLocalVariables	LPC 函式允許的區域變數個數
	MaxCallDepth		LPC 函式允許遞迴呼叫的深度
	ArgumentsInTrace	是否在 trace 訊息中列出函式參數值
	LocalVariablesInTrace	是否在 trace 訊息中列出區域變數值

   詳細說明請參考 neolith.conf 中的註解。

 - 預設標頭檔 (GlobalInclude) 不存在時, 不再產生編譯錯誤。

Neolith-0.1.1 變動摘要
======================

 - 不再允許使用 call_other 形式呼叫 static、private 或 protected 函數。

   理由如下:

   原 MudOS 允許在特定情況下使用 call_other 呼叫 static 與 private 函數(!?)
   ，這個情況是如果 call_other 的對象和 current object 相同。 (可能很多巫師
   不知道這一點) 允許這種呼叫會允許一種木馬攻擊方式, 在任何物件中:

   object ob;
   function f = (: call_other, $(ob), "any_private_function" :);

   如果攻擊者能夠將 f 成功地 bind 在 ob 上, 就可以從任何地方呼叫 ob 的私有
   函數, 而且 ob 的程式設計者完全沒有防止的辦法。

   在使用 function pointer 的 mudlib 中, 如果一個物件要「有條件」地允許其他
   物件呼叫自己的私有函數, 應該要「自己建立」這個 function pointer, 並且將
   之 bind 在它允許呼叫的物件上，也就是在 ob 的程式中必須明確寫有:

   function f = bind ((: some_private_function :), granted_object );

   才能將一個私有函數曝露給其他物件。

   封閉 call_other 呼叫 static 與 private 的函數功能讓私有函數可以真正的被
   私有，並且簡化 valid_bind 安全檢查的設計。

   其他影響: 某些使用函數名稱指定讓 driver 呼叫的函數, 如 sort_array, filter
   等傳入的函數名稱, 如果是 private 函數, 會失去效用, 請改成用 (: func :)
   形式, 這種形式速度會比原來使用字串的形式速度快一點。

 - efun time() 的傳回值改為在同一個控制流程 (control flow) 中不會改變。換句
   話說對 LPC 而言，不管機器 CPU 的速度快還是慢，在 LPC 將控制權交還給
   neolith 之前，time() 的傳回值都不會變。

   理由如下:

   原 MudOS 的 time() 會在每次被執行時呼叫真正的系統呼叫 time(2)，一般而言
   每個 LPC 控制流程執行時間都不會超過一秒鐘，但在某些情況下，例如用 LPC
   執行使用者存檔清查的迴圈，可能會用 set_eval_limit 強制執行需要較長時間
   的功能。在這種情形下，如果執行時間超過一秒鐘，time() 會傳回不同的時間。

   但是 MudOS 中其他還有許多和時間相關的 efun 如 query_idle, call_out, 等
   使用的是 backend 的 current_time 為計算基準，而傳統上 mudlib 中這些函
   數的時間會拿來和 time() 比較, time() 在用途上比較接近一個時間戳記
   (timestamp) 而非真正的時間，因此將 time() 的傳回值改為傳回 backend 的
   current_time，比較不會發生意料之外的情形。

   這同時也可以避免一些發生在秒和秒邊界的問題。

 - 新增支援 TELNET 通訊協定 RFC-1091 列編輯模式 (LINEMODE), 如果 client 端
   支援, 會打開本地列編輯模式 (即使用者輸入完一整列才送出一個封包), 使用列
   編輯模式可以大幅度降低封包數量, 讓頻寬利用更有效。

   telnet 使用者可以用 ^]display 測試自己的 telnet 程式是否支援 LINEMODE。

 - 完全移除 swap 機制。

   理由如下:

   以 ES2 實際測試結果, 300+ 人上線時, 約使用 15M 記憶體, swap 只移出不到
   300K, 效果不是很明顯。

   移除 swap 可以得到不少效能的提升, 並且在每個 object_t 少掉一個長整數
   swap_num (4 bytes), 在每個 program_t 少掉一個整數 line_swap_index
   (4 bytes) 的記憶體空間。

   300+ 人上線時 object 約 10000 個; program 約 2000 個, 移除 swap 共省下
   約 50K。

   以約 250K 的記憶體換提高的效率, 划算。

 - 以下 efun 新增不傳入任何參數的呼叫方式，此時會以 this_object() 為預設值:

   deep_inventory()
   environment()
   living()
   query_snoop()
   query_snooping()
   remove_interactive()
   userp()
   virtualp()

 - 新增使用者指令後面多餘的空白字元 (' ', \t, \v, \r, \f, \n) 在傳送給指
   令處理函式之前會自動被刪除掉。 (MudOS 只會刪除 ' ')

 - 變更 master object 的 get_root_uid() 與 get_bb_uid() 為選擇性提供, 如
   果 mudlib 不提供這兩個函式, neolith 仍可執行, 但 master object 與
   backbone 的 uid 將會是 NONAME。

 - 改良 cp() 檢查讀寫權的方式，現在 cp() 會先對第一個檔名呼叫 valid_read，
   若通過才會對第二個檔名呼叫 valid_write。

 - 刪除以下 Efun:

   terminal_colour

 - 修正物件在 reset() 中對自己 call_other()，導致 clean_up() 永遠不會被呼
   叫的問題，現在 reset() 與 clean_up() 的呼叫時機和說明文件所描述的一致。

 - 變更傳入 master object 的所有 valid_* apply 的檔名，現在不再帶有副檔名
   了，如果您的 mudlib 依賴某些 include 檔中定義的檔名 (如 ES2 mudlib)，
   像是 USER_OB、MASTER_OB 之類的，您必須將這些定義中的副檔名刪去才能維持
   原來的功能。例如: /include/globals.h 中定義:

   #define MASTER_OB	"/adm/obj/master.c"

   應改為:

   #define MASTER_OB	"/adm/obj/master"

 - 變更 user applies 函式名稱 'terminal_type' 為 'set_terminal_type' 以及
   'window_size' 為 'set_window_size'。

Neolith-0.1.0 變動摘要
======================

 - 更新 runtime config 檔的語法，並新增若干個新的設定項目，請參考
   src/neolith.conf 的範例及其中的註解說明。另外請使用 LPC/ 目錄下的
   runtime_config.h 取代您 mudlib 裡舊版的這個檔案。

 - 重新安排原始碼目錄結構，將原 MudOS 的原始碼依照功能分為數類各自以獨立的
   子目錄存放。

$Id: NEWS,v 1.2 2002/11/25 11:10:55 annihilator Exp $
