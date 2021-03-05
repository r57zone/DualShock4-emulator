[![EN](https://user-images.githubusercontent.com/9499881/33184537-7be87e86-d096-11e7-89bb-f3286f752bc6.png)](https://github.com/r57zone/DualShock4-emulator/) 
[![RU](https://user-images.githubusercontent.com/9499881/27683795-5b0fbac6-5cd8-11e7-929c-057833e01fb1.png)](https://github.com/r57zone/DualShock4-emulator/blob/master/README.RU.md)
# DualShock4 emulator
Простое приложение для эмуляции геймпада Sony DualShock 4, с помощью Xbox контроллера или клавиатуры и мыши. Данный способ необходим для полноценной работы сервиса [Sony Playstation Now](https://www.playstation.com/en-us/explore/playstation-now/) или Playstation Remote Play. Работает на базе драйвера [ViGEm](https://github.com/ViGEm).

## Настройка
1. Установить [ViGEmBus](https://github.com/ViGEm/ViGEmBus/releases).
2. Установите Microsoft Visual C++ Redistributable 2017 или новее.
3. Распаковать и запустить DualShock4 emulator.
4. Запустить "PlayStation Now" или другое приложение.
5. При необходимости можно инвертировать оси, измените параметры `InvertX` и `InvertY` на `1`, в конфигурационном файле "Config.ini", в разделе "DS4".

## Xbox контроллер
Кнопка "Назад" на Xbox контроллере эмулирует нажатие тачпада на Sony DualShock4.

Кнопка "Поделиться" назначена на клавишу "F12".



При необходимости можно обменять местами бамперы и триггеры, а также кнопку "поделится" и нажатие тачпада, для этого в конфигурационном файле "Config.ini" нужно изменить параметр `SwapTriggersShoulders` или `SwapShareTouchPad` на `1`.

## Тачпад
Игра | Действие
------------ | -------------
Uncharted 3: Иллюзии Дрейка (2011) | Кнопка "Share" (F12) дублирует нажатие левой части тачпада.

Свайпы тачпада пока не поддерживаются, вы не сможете поиграть в такие игры, как: Until Dawn.

## Гироскоп
Эмуляция гироскопа не поддерживается.

## Клавиатура и мышь
По умолчанию мышь и клавиатура работает только в "PlayStation Now". Можно включить работу во всех окнах (измените параметр `ActivateInAnyWindow` на `1`, в конфигурационном файле "Config.ini") или измените название окна (параметр `ActivateOnlyInWindow`), в котором захватываются действия. Это необходимо, чтобы курсор центрировался только в одном окне и не нажимались кнопки при свёрнутом окне.

DualShock 4 | Клавиатура и мышь
------------ | -------------
L1 | Control
R2 | Alt
L2 | Правая кнопка мыши
R2 | Левая кнопка мыши
SHARE | F12
TOUCHPAD (нажатие тачпада) | Enter
OPTIONS | Tab
DPAD UP (стрелка вверх) | 1
DPAD LEFT (стрелка влево) | 2
DPAD RIGHT (стрелка вправо) | 3
DPAD DOWN (стрелка вниз) | 4
TRIANGLE (треугольник) | Q
SQUARE (квадрат) | E
CIRCLE (круг) | R
CROSS (крестик) | Space
L3 (нажатие стика) | Shift
R3 (нажатие стика) | Средняя кнопка мыши

В конфигурационном файле "Config.ini" можно заменить привязки кнопок. Коды кнопок можно найти [здесь](https://github.com/r57zone/Half-Life-Alyx-novr/blob/master/BINDINGS.RU.md). 



Параметры чувствительности `SensX`, `SensY` для мыши можно также найти в конфигурационном файле, в разделе "Mouse". При отсуствии движения стика можно попробовать увеличить параметр "SleepTimeOut" до 2, 4, 8, 10.



Также можно включить эмуляцию аналоговых триггеров (L2, R2), измените параметр `EmulateAnalogTriggers` на `1` и шаг увеличения `AnalogTriggerStep` (от 0.1 до 255).

## Загрузка
>Версия для Windows 10.

**[Загрузить](https://github.com/r57zone/DualShock4-emulator/releases)**

## Обратная связь
`r57zone[собака]gmail.com`