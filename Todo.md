# NanoEFI 实现审查 TODO

目标：完整理解运行机制（预计 30~45 min）

---

## 执行路径（从 main 到 driver Entry）

```
NanoEfiBackendInit(&svc)
  └─ NefHalInit()
  └─ 填充 svc 22个函数指针 (BkXxx → NefHalXxx)

NefAllocExec(sz)           ← mmap(PROT_EXEC) / VirtualAlloc(RWX)
fread(img, sz, f)

NefLoad(img, sz, &svc, arg, malloc, free, Open)
  1. magic == "NEFI"
  2. Hdr->Version <= NDIH_VERSION
  3. Hdr->ArchId == 当前平台
  4. EntryOffset < ImageSize
  5. ImageSize <= 传入 sz
  6. Hdr->MinSvcVersion <= svc.Version
  7. CRC32(img[0..sz-5]) == img[sz-4..sz-1]
  alloc(DataSize + BssSize)
  memset(.bss, 0)
  Entry = (NanoEfiDriverEntry)(img + EntryOffset)
  return Entry(&svc, arg)
```

---

## 审查顺序

- [ ] `loader/NefLoader.c` — 10 min：7步校验顺序和错误码
- [ ] `backends/NefBackend.c` — 5 min：shim 模式 + va_list 传递
- [ ] `backends/hal/linux/NefHalLinux.c` — 15 min：pthread 对象分配/释放，MakeDeadline
- [ ] `cmake/NanoEFI.cmake` (add_nef_driver) — 10 min：clang flat binary → ndih_pack 流水线

---

## 可跳过

- `lib/NefString.c` / `lib/NefFormat.c` — 标准 freestanding 实现，无悬念
- `backends/linux/NefLinux.c` / `backends/win32/NefWin32.c` — 旧后端，已被 HAL 替代
