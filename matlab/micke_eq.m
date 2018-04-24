clearvars
clear all
close all

[x, fsx] = audioread('before2.wav');
[y, fsy] = audioread('after2.wav');

% cut silence
x = x(fsx * 2 : fsx * 3.5);
y = y(fsy * 2 : fsy * 3.5);

% create spectrum
[Px, fx] = pwelch(x, [], [], length(x), fsx);
[Py, fy] = pwelch(y, [], [], length(y), fsy);
powPx = pow2db(Px);
powPy = pow2db(Py);

xLin = 1:0.01:4.5;
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

min_total = min([min(powPx), min(powPy)]) - 3;
max_total = max([max(powPx(2:length(powPx))), max(powPy(2:length(powPx)))]) + 3;

x_mean = mean(powPx);
y_mean = mean(powPy);


ax = subplot(3, 1, 1);
plot(ax,fx, powPx);
axis(ax,[44, 22720, min_total, max_total]);
set(ax, 'XScale', 'log')
title(ax, 'Before');
ylabel(ax, 'Magnitude (dB)');
xlabel(ax, 'Frequency (Hz)');
grid on

ay = subplot(3, 1, 2);
plot(ay, fy, powPy);
axis(ay, [44, 22720, min_total, max_total]);
set(ay, 'XScale', 'log')
title(ay, 'After');
ylabel(ay, 'Magnitude (dB)');
xlabel(ay, 'Frequency (Hz)');
grid on

%Adds a combined plot of both curves for comparing
both = subplot(3, 1, 3);
plot(both, fx, powPx, 'r');
hold on
plot(both, fy, powPy, 'g');
axis(both, [44, 22720, min_total, max_total]);
set(both, 'XScale', 'log');
title(both, 'Combined');
ylabel(both, 'Magnitude (dB)');
xlabel(both, 'Frequency (Hz)');
grid on