# Лабораторная работа 4, вариант 20

Родитель создаёт два процесса программы child. Строки длиной до 10 символов включительно получает первый ребёнок, длиннее 10 — второй. Дети разворачивают строки и записывают в свои файлы. Обмен идёт через mmap, синхронизация — через SIGUSR1 и SIGUSR2. Входной текст предполагается английским (ASCII): один символ занимает один байт.

## Сборка

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Запуск

```bash
./build/parent
```

Первые две строки ввода — разные имена выходных файлов, затем строки для обработки. Завершить ввод: Ctrl+D на пустой строке. Выходные файлы очищаются при запуске.

Готовый пример:

```bash
./build/parent < input.txt
cat short.txt long.txt
cmp expected_short.txt short.txt
cmp expected_long.txt long.txt
```

## Трассировка

```bash
strace -f -tt -yy -s 160 -o strace.log ./build/parent < input.txt
```

Полный лог находится в strace.log. После нового запуска PID и адреса изменятся.

## Файлы

- src/, include/ — исходники и заголовки.
- CMakeLists.txt — сборка parent и child.
- input.txt — имена выходов и строки примера.
- short.txt, long.txt — результаты.
- expected_short.txt, expected_long.txt — ожидаемые результаты для сравнения.
- strace.log — полная трасса запуска.
- tests/run_tests.py — проверки; запуск: python3 tests/run_tests.py.
- run_demo.sh — запуск и проверка примера.
- run_strace.sh — получение strace.log.

Скрипты .sh необязательны: те же команды приведены выше. После переноса проекта нужно заново запустить CMake и сборку.
