# Brigadier C++

![License](https://img.shields.io/badge/License-MIT-blue) ![C++20](https://img.shields.io/badge/C%2B%2B20-Standard-purple)

**A modern C++20 port of Mojang's Brigadier command parser, with rich argument types, tab-completion, error reporting, and a fluent builder API.**

**Mojang Brigadier 命令解析库的现代化 C++20 移植，提供丰富的参数类型、自动补全、错误提示与流式构建接口。**

---

## Platforms / 平台支持

- Windows / Linux / macOS
- Requires a C++20 compiler (MSVC 19.29+, GCC 11+, or Clang 14+)
- Uses `std::wstring` internally for full Unicode command text, and returns suggestions as `std::future<Suggestions>`

需要支持 C++20 的编译器（MSVC 19.29+ / GCC 11+ / Clang 14+）。内部统一使用 `std::wstring` 处理命令文本，自动补全接口返回 `std::future<Suggestions>`。

---

### Minimal example / 最小示例

```cpp
#include "brigadier.hpp"

int main() {
	// 1. 指定命令来源类型（可以是任意类型，例如 void*、一个自定义 Player 结构等）
	using S = void *;

	// 2. 创建分发器
	brigadier::CommandDispatcher<S> dispatcher;

	// 3. 注册字面量命令，通过链式 Then / Executes 构建语法树
	dispatcher.Register(L"help").Executes([](brigadier::CommandContext<S> &ctx) {
		std::wcout << L"可用命令:\n";
		for (auto &s : dispatcher.GetAllUsage(dispatcher.GetRoot().get(), nullptr, false))
			std::wcout << L"  " << s << L'\n';
		return 1;
	});

	dispatcher.Register(L"to")
		.Then<brigadier::Argument, brigadier::UINT16>(L"进制", 2, 36)  // 2..36 的进制参数
		.Then<brigadier::Argument, brigadier::Integer>(L"十进制数", 0) // 非负整数
		.Executes([](auto &ctx) {
			auto r = ctx.GetArgument<brigadier::UINT16>(L"进制");
			auto v = ctx.GetArgument<brigadier::Integer>(L"十进制数");
			// ... 转成 r 进制
			return 1;
		});

	// 4. 解析并执行
	try {
		dispatcher.Execute(L"to 2 42", nullptr);
	} catch (const brigadier::CommandSyntaxException &ex) {
		std::wcerr << L"错误: " << ex.What() << L'\n';
	}
}
```

---

## API Reference / 接口参考

### CommandDispatcher&lt;S&gt;

分发器是整个库的核心，`S` 表示命令来源（source）类型，例如一个玩家对象、一个控制台句柄或者 `void *`。

| 方法 | 说明 |
|---|---|
| `Register(args...)` | 注册一个顶层节点（默认 `Literal`），返回一个构建器（builder） |
| `Register<Next, Type>(args...)` | 显式指定顶层节点类型（如 `Argument, Integer`） |
| `Parse(input, source)` | 仅解析，返回 `ParseResults<S>`，即使失败也不抛异常 |
| `Execute(input, source)` | 解析并执行，失败时抛出 `CommandSyntaxException` |
| `Execute(ParseResults<S>&)` | 执行预先解析的结果 |
| `GetCompletionSuggestions(parse, cursor, cancel)` | 获取自动补全建议（返回 `std::future<Suggestions>`） |
| `GetRoot()` | 获取根节点指针 |
| `GetAllUsage(node, source, restricted)` | 列出指定节点下所有完整用法字符串 |
| `GetSmartUsage(node, source)` | 列出压缩后的智能用法字符串 |
| `GetPath(node)` / `FindNode(path)` | 在树中定位节点 / 通过路径找到节点 |
| `FindAmbiguities(consumer)` | 扫描潜在歧义 |
| `SetConsumer(consumer)` | 设置执行结果回调 |

### Building commands / 构建命令

构建器 API 是流式的，用于拼装命令语法树：

```cpp
dispatcher.Register(L"stop").Executes([](auto &) { std::exit(0); return 1; });

dispatcher.Register(L"give")
	.Then<brigadier::Argument, brigadier::String>(L"物品")   // 一个可引用字符串
	.Then<brigadier::Argument, brigadier::Integer>(L"数量", 1) // 至少 1
	.Requires([](auto &source) { return source.isAdmin(); })   // 谓词：只有管理员能用
	.Executes([](auto &ctx) {
		auto item = ctx.GetArgument<brigadier::String>(L"物品");
		auto count = ctx.GetArgument<brigadier::Integer>(L"数量");
		/* ... */
		return 1;
	});
```

| 方法 | 说明 |
|---|---|
| `Then<Next, Type>(args...)` | 添加子节点；`Next` 通常是 `Literal` 或 `Argument`，`Type` 是参数类型 |
| `ThenOptional<Next, Type>(args...)` | 添加一个“可选子节点”，返回 `MultiArgumentBuilder`，可同时作用于父/子节点 |
| `Then(std::shared_ptr<Node>)` | 通过共享指针添加已存在的节点 |
| `Executes(callback)` | 设置该节点被匹配时执行的回调，返回 `int` |
| `Requires(predicate)` | 设置该节点的可用性谓词，返回 `bool` |
| `Redirect(target)` / `Fork(target, modifier)` | 重定向到另一个节点，或分叉出一组来源 |
| `MultiArgumentBuilder::Executes(cb, only_master)` | 在多节点构建器上设置回调，`only_master=false` 时同时作用于所有节点 |

### Argument types / 参数类型

所有内置参数类型都是 `ArgumentType<T>` 的派生类，`T` 是解析后的结果类型。

| 别名 | 解析结果 | 说明 |
|---|---|---|
| `brigadier::Word` | `std::wstring` | 单个未加引号的单词 |
| `brigadier::String` | `std::wstring` | 可加引号的短语 |
| `brigadier::GreedyString` | `std::wstring` | 贪婪读取剩余所有内容 |
| `brigadier::Bool` | `bool` | `true` / `false` |
| `brigadier::Char` | `wchar_t` | 单个字符 |
| `brigadier::Float` / `Double` | `float` / `double` | 浮点数 |
| `brigadier::Integer` / `Long` | `int` / `long long` | 有符号整数 |
| `brigadier::INT16` / `UINT16` / `INT32` / `UINT32` / `INT64` / `UINT64` | 对应定宽整型 | 定宽整数 |
| `brigadier::Number<T>` | `T` | 任意算术类型模板 |
| `brigadier::Enum<T>` | `T` | 需要 `magic_enum.hpp`，自动补全枚举名 |

例如：

```cpp
dispatcher.Register(L"to")
	.Then<brigadier::Argument, brigadier::UINT16>(L"进制", 2, 36)         // 上下界 2..36
	.Then<brigadier::Argument, brigadier::Double>(L"数值", -1.0, 1.0);   // 上下界 -1.0..1.0
```

### CommandContext&lt;S&gt;

`CommandContext` 在执行回调中携带了解析后的结果与来源。

| 方法 | 说明 |
|---|---|
| `GetSource()` | 取回当前的 source 对象 |
| `GetArgument<Type>(name)` | 按名称取出指定类型的参数（类型不匹配会抛出） |
| `GetArgumentOr<Type>(name, default)` | 不存在时返回默认值 |
| `GetRootNode()` / `GetNodes()` | 命令树的相关节点信息 |
| `GetRange()` / `GetInput()` | 命令文本中匹配的范围与原始输入 |
| `HasNodes()` / `IsForked()` | 是否含节点 / 是否处于分叉路径 |
| `GetChild()` / `GetParent()` | 上下文链 |

### StringReader

`StringReader` 是解析期间使用的输入游标。

```cpp
brigadier::StringReader reader(L"3 ff00ff");
reader.CanRead();                 // true
wchar_t c = reader.Read();        // 读取并前进
wchar_t p = reader.Peek();        // 读取但不前进
reader.Skip();                    // 前进一位
int cursor = reader.GetCursor();
reader.SetCursor(cursor);
auto n = reader.ReadValue<int>(); // 按 T 的类型解析
auto s = reader.ReadString();     // 读取引号字符串或未加引号字符串
reader.Expect(L' ');              // 期望字符，否则抛出
```

### Suggestions / 自动补全

补全通过 `ListSuggestions` 返回一个 `std::future<Suggestions>`，可以异步完成。

```cpp
dispatcher.GetCompletionSuggestions(parse, cursor, &cancel).then(...);
```

内置类型在补全时会自动给出匹配项，例如 `Bool` 会给出 `true` / `false`；自定义类型可以在 `ListSuggestions` 中构建补全：

```cpp
static std::future<brigadier::Suggestions> ListSuggestions(
		brigadier::CommandContext<S> &ctx,
		brigadier::SuggestionsBuilder &builder) {
	builder.Suggest(L"red",   L"红色");
	builder.Suggest(L"green", L"绿色");
	builder.AutoSuggest(L"blue", builder.GetRemainingLowerCase());
	return builder.BuildFuture();
}
```

`SuggestionsBuilder` 的常用方法：

| 方法 | 说明 |
|---|---|
| `Suggest(text)` / `Suggest(text, tooltip)` | 添加一个候选项，可附带提示 |
| `AutoSuggest(text, input)` | 若 `text` 以 `input` 为前缀则添加（大小写敏感） |
| `AutoSuggestLowerCase(container)` | 对容器里每一项做小写匹配自动建议 |
| `Build()` / `BuildFuture()` | 构造 `Suggestions` / `std::future<Suggestions>` |
| `SetOffset(start)` / `Restart()` | 调整起点 / 清空缓存 |

### Errors / 错误处理

所有解析错误都以 `CommandSyntaxException` 抛出，携带原始输入、游标位置与人类可读的消息。

```cpp
try {
	dispatcher.Execute(L"to 99 10", nullptr);
} catch (const brigadier::CommandSyntaxException &ex) {
	std::wcerr << L"错误: " << ex.What() << L'\n';
	std::wcerr << L"  在: " << ex.GetInput() << L'\n';
	std::wcerr << L"  位置: " << ex.GetCursor() << L'\n';
}
```

`BuiltInExceptions` 提供了常见错误构造器，例如：

- `ReaderExpectedValue` / `ReaderExpectedSymbol`
- `ReaderInvalidValue` / `ReaderInvalidEscape`
- `ValueTooLow` / `ValueTooHigh`
- `DispatcherUnknownCommand` / `DispatcherUnknownArgument`
- `DispatcherParseException`

---

## Custom argument types / 自定义参数类型

继承 `ArgumentType<T>` 并实现 `Parse`，同时（可选地）提供 `GetTypeName`、`GetExamples` 和 `ListSuggestions`：

```cpp
class LType : public brigadier::ArgumentType<std::tuple<Integer, std::wstring>> {
public:
	std::tuple<Integer, std::wstring> Parse(brigadier::StringReader &reader) {
		// ... 解析逻辑
	}

	static constexpr std::wstring_view GetTypeName() { return L"Number"; }

	static std::vector<std::wstring> GetExamples() { return { L"16 ff", L"2 1010" }; }

	template<typename S>
	static std::future<brigadier::Suggestions> ListSuggestions(
			brigadier::CommandContext<S> &ctx,
			brigadier::SuggestionsBuilder &builder) {
		return brigadier::Suggestions::Empty();
	}
};

// 使用：
dispatcher.Register(L"from")
	.Then<brigadier::Argument, brigadier::UINT16>(L"进制", 2, 36)
	.Then<brigadier::Argument, LType>(L"N进制数")
	.Executes([](auto &ctx) {
		auto R = ctx.GetArgument<brigadier::UINT16>(L"进制");
		auto V = ctx.GetArgument<LType>(L"N进制数");
		// std::get<0>(V) 是数值，std::get<1>(V) 是原始字符串
		return 1;
	});
```

实现要点：

- `Parse` 接收一个 `StringReader &`，成功时返回 `T`，失败时抛出 `CommandSyntaxException`（推荐用 `BuiltInExceptions` 构造）。
- `GetTypeName` 会出现在错误信息与用法字符串（usage）里。
- `GetExamples` 用于歧义检测。
- 若想让类型在补全时给出候选，实现 `ListSuggestions` 即可。

---

## Advanced features / 高级特性

### Fork / Redirect

```cpp
// 把命令转发到另一个节点执行
dispatcher.Register(L"say")
	.Then<brigadier::Argument, brigadier::GreedyString>(L"内容")
	.Fork(dispatcher.GetRoot(), [](auto &ctx) {
		return ctx.GetSource().getListeners();   // 返回一组新的 source
	});
```

- `Redirect` 会把后续解析切换到目标节点，来源不变。
- `Fork` 会为每个新的 source 重新执行一次命令，返回值是成功执行的数量。

### Ambiguity detection / 歧义检测

```cpp
dispatcher.FindAmbiguities([](auto *parent, auto *child, auto *sibling, std::set<std::wstring> &inputs) {
	std::wcerr << L"歧义: " << child->GetName() << L" 与 " << sibling->GetName() << L'\n';
});
```

### Usage generation / 用法字符串

```cpp
// 完整用法（每个命令一行）
auto full = dispatcher.GetAllUsage(dispatcher.GetRoot().get(), source, /*restricted=*/false);

// 智能用法（同层压缩，例如 `foo [<int>]`）
auto smart = dispatcher.GetSmartUsage(dispatcher.GetRoot().get(), source);
```

### Result consumer / 结果回调

```cpp
dispatcher.SetConsumer([](brigadier::CommandContext<S> &ctx, bool success, int result) {
	// 每次命令执行后都会被调用一次，无论成功或失败
});
```

---

## Third-party optional integration / 可选第三方集成

如果头文件所在目录里能找到以下文件，会自动启用对应功能：

- `nameof.hpp` — 通过 `NAMEOF_TYPE` 推断参数类型名，用于错误信息。
- `magic_enum.hpp` — 启用 `brigadier::Enum<T>` 类型，可解析和补全枚举值。

---

## License / 许可证

MIT License.

Copyright (c) 2022 Tomasz Karpiñski
Copyright (c) 2026 KinnerFisch
