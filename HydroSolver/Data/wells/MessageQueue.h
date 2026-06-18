#pragma once
#include <memory>
#include <list>
#include <mutex>
#include <optional>
#include <vector>

namespace IPC {
	class MessageQueue {
	public:
		/// <summary>
		/// Отправляет сообщение в очередь
		/// </summary>
		/// <typeparam name="T">Любой тип для которого sizeof возвращает валидное значение</typeparam>
		/// <param name="src">От кого</param>
		/// <param name="dest">Кому</param>
		/// <param name="content">Данные</param>
		template<typename T> void Send(int src, int dest, T&& content)
		{
			auto ptr = std::make_unique<uint8_t[]>(sizeof(T));
			std::memmove(ptr.get(), &content, sizeof(T));
			send(src, dest, std::move(ptr));
		}
		/// <summary>
		/// Получает сообщение
		/// </summary>
		/// <typeparam name="T">Любой тип для которого sizeof возвращает валидное значение</typeparam>
		/// <param name="to">Для кого</param>
		/// <param name="from">От кого. Если -1 то значение игнорируется</param>
		/// <returns>nullopt если сообщений нет или значение</returns>
		template<typename T> std::optional<T> Get(int to, int from)
		{
			auto res = get(to, from);
			if (res.has_value())
				return std::make_optional<T>((*(T*)(res.value().get())));
			else
				return std::nullopt;
			//return (res.has_value()) ? (*(T*)(res.value().get())) : std::nullopt;
		}

	private:
		struct msg
		{
			int src;
			int dest;
			std::unique_ptr<uint8_t[]> content;
		};
		std::list<msg> queue;
		std::mutex queueLock;

		void send(int src, int dest, std::unique_ptr<uint8_t[]> content);
		std::optional<std::unique_ptr<uint8_t[]>> get(int to, int from);
	};

	/*
	* 1) Все потоки запущены, ждем когда потоки MapCalculation досчитают
	* флаг завершения - IsMapCalculationFinished
	* 2)Посылаем команду завершения потоку GPUConveyor
	* он получит её тогда когда все данные уже обработа - то есть он будет в конце очереди
	* 3)Когда GPUConveyor отчитается о завершении то все сообщения от него уже будут в очереди
	* Посылаем команду заверщения потоку DBHandler
	* 4)После завершения DBHandler завершаем консоль
	*/
	class MapCalculationSync {
	public:
		void RegisterID();
		void Run(std::shared_ptr<MessageQueue> queue);
	private:	

		void WaitMapCalculationFinish(std::shared_ptr<MessageQueue> queue);
		template<int Sender> void WaitFinish(std::shared_ptr<MessageQueue> queue)
		{
			queue->Send(0, Sender, 'c');  //отправляем команду close
			bool stop = false;
			while (!stop)
			{
				auto done_val = queue->Get<char>(0, Sender);
				if (done_val.has_value())
				{
					stop = true;
				}

				//засыпаем на 1 мс
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
		

		int ids = 0;
	};
}