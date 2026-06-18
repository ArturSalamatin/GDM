#include "MessageQueue.h"
/// <summary>
/// Отправляет сообщение в очередь
/// </summary>
/// <param name="src">Номер отправителя</param>
/// <param name="dest">Номер получателя</param>
/// <param name="content">Указатель на данные</param>
__declspec(noinline) void IPC::MessageQueue::send(int src,int dest, std::unique_ptr<uint8_t[]> content)
{
	std::lock_guard lock(queueLock);
	queue.push_back({ src, dest, std::move(content) });
}
/// <summary>
/// Получает сообщение из очереди
/// </summary>
/// <param name="to">Номер получателя</param>
/// <param name="from">Номер отправителя. Если -1 то отправитель не важен</param>
/// <returns>Указатель на данные</returns>
__declspec(noinline) std::optional<std::unique_ptr<uint8_t[]>> IPC::MessageQueue::get(int to, int from)
{
	std::lock_guard lock(queueLock);

	auto& val = *queue.begin();
	if (val.dest == to)
	{
		if (val.src == from || from == -1)
		{
			auto new_ptr = std::move(val.content);
			queue.erase(queue.begin());
			return new_ptr;
		}
		else {
			return std::nullopt;
		}
	}
	else {
		return std::nullopt;
	}
}

void IPC::MapCalculationSync::RegisterID()
{
	ids++;
}
void IPC::MapCalculationSync::Run(std::shared_ptr<MessageQueue> queue)
{
	WaitMapCalculationFinish(queue);
	WaitFinish<10>(queue); //GPUConveyor
	WaitFinish<12>(queue); //DBHandler
	WaitFinish<13>(queue); //Console
}
/// <summary>
/// Ожидаем когда все зарегистрированные MapCalculation потоки завершат работу
/// </summary>
/// <param name="queue">Очередь</param>
void IPC::MapCalculationSync::WaitMapCalculationFinish(std::shared_ptr<MessageQueue> queue)
{
	int entries = 0;
	while (entries != ids)
	{
		auto done_val = queue->Get<int>(0, 11);
		if (done_val.has_value())
		{
			entries++;
		}

		//засыпаем на 1 мс
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}




