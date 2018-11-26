clearvars
clear all
close all

[x, fsx] = audioread('before.wav');
[y, fsy] = audioread('after.wav');

sound_start_sec = 3 * fsx;
sound_stop_sec = 30 * fsx;

if fsx ~= fsy
    return
end

if sound_stop_sec > length(x)
    sound_start_sec = 1;
    sound_stop_sec = length(x);
end

% cut silence
x = x(sound_start_sec : sound_stop_sec);
y = y(sound_start_sec : sound_stop_sec);

% create spectrum
N = 65536;
[Px, fx] = pwelch(x, [], [], N, fsx);
[Py, fy] = pwelch(y, [], [], N, fsy);

% Smoothen pink noise by * f
for i = 1:length(Px)
    Px(i) = Px(i) * fx(i);
end

for i = 1:length(Py)
    Py(i) = Py(i) * fy(i);
end

powPx = pow2db(Px);
powPy = pow2db(Py);

xLin = 0:0.015:log10(length(fx));
for i = 1:length(xLin)
    xLog(i) = round(10^xLin(i));
end

for i = 2:length(xLog)-1
   newPowPx(i) = mean(powPx(xLog(i-1):xLog(i+1)));
end

fx = fx(xLog(2:length(xLog)));
powPx = newPowPx;

for i = 2:length(xLog)-1
   newPowPy(i) = mean(powPy(xLog(i-1):xLog(i+1)));
end

fy = fy(xLog(2:length(xLog)));
powPy = newPowPy;

all_index = find(fx >= 19 & fx <= 20500);
% fx = fx(all_index);
% fy = fy(all_index);
% powPx = powPx(all_index);
% powPy = powPy(all_index);

min_total = min([min(powPx(all_index)), min(powPy(all_index))]) - 3;
max_total = max([max(powPx(all_index)), max(powPy(all_index))]) + 3;

% Adds a combined plot of both curves for comparing
both = subplot(1, 1, 1);
plot(both, fx, powPx, 'r');
hold on
plot(both, fy, powPy, 'g');
axis(both, [20, 20000, min_total, max_total]);
set(both, 'XScale', 'log');
title(both, 'Combined');
ylabel(both, 'dB');
xlabel(both, 'Hz');
grid on

x0=0;
y0=0;
width=1920;
height=300;

set(gcf,'units','points','position',[x0,y0,width,height])
