#!/bin/bash
set -e

echo "Генерация elems.txt"
./elements

echo "Генерация satellite_1_orbit.csv"
./earth 5 1

echo "Перевод координат(x,y,z) в долготу и широту"
./XYtoPL satellite_1_orbit.csv

echo "Отрисовка трассы"
./draw_line input.jpg points.txt 1

echo "Всё готово!"
