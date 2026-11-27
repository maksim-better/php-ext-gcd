# ext-gcd 文档（中文）

[English](../en/README.md) | [中文](../zh/README.md)

在 ZTS PHP 上提供 GCD（Grand Central Dispatch）风格的并发：把闭包派发到由真实操作系统线程池支撑的串行/并发队列，用 group 与 semaphore 同步，监听事件源，并通过线程安全容器共享状态。

## 指南

1. [快速开始](guide/01-quickstart.md) —— 安装、启用、第一个并发任务
2. [核心概念](guide/02-concepts.md) —— 队列、线程模型、任务规则
3. [派发任务](guide/03-dispatch.md) —— async/sync、barrier、apply、once
4. [同步原语](guide/04-sync.md) —— group 与 semaphore
5. [事件源](guide/05-sources.md) —— 定时器、信号、子进程、文件描述符
6. [数据缓冲](guide/06-data.md) —— dispatch_data 与 fd 读写
7. [共享容器](guide/07-containers.md) —— Atomic、Map、Set、ConcurrentQueue
8. [错误处理](guide/08-errors.md) —— 处理器与异常
9. [QoS 与主循环](guide/09-qos-mainloop.md) —— 优先级、main()/drain()

## API 参考

- [API 索引](api/README.md) —— 全部函数、类、常量、异常
- [队列与 QoS](api/queue.md)
- [派发](api/dispatch.md)
- [Group](api/group.md)
- [Semaphore](api/semaphore.md)
- [事件源](api/source.md)
- [数据](api/data.md)
- [主循环](api/loop.md)
- [错误](api/errors.md)
- [容器](api/containers.md)