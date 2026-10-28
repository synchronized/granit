<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Window API 迁移

## 窗口标志宏

窗口标志宏已统一增加 `FLAG` 语义。将旧名称替换为对应的新名称：

| 旧名称 | 新名称 |
| --- | --- |
| `GRANIT_WINDOW_VISIBLE_BIT` | `GRANIT_WINDOW_FLAG_VISIBLE_BIT` |
| `GRANIT_WINDOW_RESIZABLE_BIT` | `GRANIT_WINDOW_FLAG_RESIZABLE_BIT` |
| `GRANIT_WINDOW_HIGH_DPI_BIT` | `GRANIT_WINDOW_FLAG_HIGH_DPI_BIT` |

C++ 用户无需修改 `granit::window_flag` 的枚举名称。

## 统一原生窗口快照

需要根据实际后端读取原生句柄时，可以使用统一快照：

```c
granit_window_native native = GRANIT_WINDOW_NATIVE_INIT;
granit_result result = granit_window_get_native(system, window, &native);
```

成功后使用 `native.backend` 判断应读取的 union 成员。快照中的指针和 Canvas selector 都是借用值，
仅在对应 Window 存活期间有效；调用方不得释放或修改它们。

C++ 用户使用 `granit::window_native` 和重载的
`granit::get_native(system, window, native)`。
