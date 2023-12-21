#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <signal.h>
#include <queue>
#include <ctime>

struct Event {
	std::string time;
	int random_number;
};

std::queue<Event> event_queue;
std::mutex queue_mutex;				//mutex для работы с очередью событий
std::mutex increase_mutex;			//mutex для инкрементирования общей переменной total_primes
std::condition_variable event_generated;
int total_primes = 0;
std::vector<std::thread> generator_threads;
std::vector<std::thread> handler_threads;

bool stop_processing = false;

//Функция проверки на простое число 
bool is_prime(int num) {
	if (num < 2) {
		return false;
	}
	for (int i = 2; i * i <= num; i++) {
		if (num % i == 0) {
			return false;
		}
	}
	return true;
}

//Функция для генерации событий
void event_generator(int num_events) {
	std::hash<std::thread::id> thread_id_converter;
	time_t now;
	srand(static_cast<unsigned>(thread_id_converter(std::this_thread::get_id()))); //Скармливаем id потока в виде seed для рандома
	for (int i = 0 ; i < num_events; i++) {
		if (stop_processing) {
			return;
		}
		
		//создаем событие
		Event event;
		event.random_number = rand() % 10000 + 1;
		now = time(0);
#pragma warning(suppress : 4996)
		event.time = ctime(&now);

		//Добавляем событие в очерель
		queue_mutex.lock();
		event_queue.push(event);
		queue_mutex.unlock();
		
		event_generated.notify_one(); // Сигнал потоку-обработчику
	}
}


void event_handler() {
	while (!stop_processing) {

		//Останавливаемся пока не появится новое событие
		std::unique_lock<std::mutex> lock(queue_mutex);
		event_generated.wait(lock, [] { return !event_queue.empty() || stop_processing; });

		//Обработка события
		while (!event_queue.empty()) {
			Event event = event_queue.front();
			event_queue.pop();

			if (is_prime(event.random_number)) {
				increase_mutex.lock();
				total_primes++;
				increase_mutex.unlock();
			}
		}
	}
}


void sigint_handler(int signal) {
	stop_processing = true;
	event_generated.notify_all();
	std::cout << "Ожидайте завершения программы...";
	for (auto& thread : generator_threads) {
		thread.join();
	}
	for (auto& thread : handler_threads) {
		thread.join();
	}
	std::cout << "\nКоличество простых чисел:" << total_primes << std::endl;
	exit(0);
}

int main() {
	setlocale(LC_ALL, "Rus");

	signal(SIGINT, sigint_handler);

	int num_threads_generator, num_threads_handler, num_events_per_thread;
	std::cout << "Задайте количество потоков-создателей: ";
	std::cin >> num_threads_generator;
	std::cout << "\nЗадайте количество событий на поток-создателя:";
	std::cin >> num_events_per_thread;
	std::cout << "\nЗадайте количество потоков обработчиков: ";
	std::cin >> num_threads_handler;

	for (int i = 1; i <= num_threads_generator; i++) {
		generator_threads.emplace_back(event_generator, num_events_per_thread);
	}
	for (int i = 0; i < num_threads_handler; i++) {
		handler_threads.emplace_back(event_handler);
	}
	for (auto& thread : generator_threads) {
		thread.join();
	}
	stop_processing = true;
	event_generated.notify_all();

	for (auto& thread : handler_threads) {
		thread.join();
	}

	std::cout << "\nКоличество простых чисел: " << total_primes << std::endl;

	return 0;
}