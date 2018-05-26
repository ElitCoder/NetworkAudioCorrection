clearvars
close all

[x, fsx] = audioread('before.wav');
[y, fsy] = audioread('after.wav');

sound_start_sec = 2;
sound_stop_sec = 30;

% cut silence
x = x(fsx * sound_start_sec : fsx * sound_stop_sec);
y = y(fsy * sound_start_sec : fsy * sound_stop_sec);

% create spectrum
N = 8192;
% [Px, fx] = pwelch(x, [], [], N, fsx);
% [Py, fy] = pwelch(y, [], [], N, fsy);
[Px, fx] = pwelch(x, N, N / 2, 'twosided', 'power');
[Py, fy] = pwelch(y, N, N / 2, 'twosided', 'power');

Px = Px(1 : N / 2);
Py = Py(1 : N / 2);
fx = fx(1 : N / 2);
fy = fy(1 : N / 2);

fx = fx * N;
fy = fy * N;

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

low_index = find(fx > 44);
min_total = min([min(powPx), min(powPy)]) - 3;
max_total = max([max(powPx(low_index)), max(powPy(low_index))]) + 3;

x_mean = mean(powPx);
y_mean = mean(powPy);

%set(0, 'DefaultAxesFontSize', 8);

%Adds a combined plot of both curves for comparing
both = subplot(1, 1, 1);
plot(both, fx, powPx, 'r');
hold on
plot(both, fy, powPy, 'g');
axis(both, [44, 22720, min_total, max_total]);
set(both, 'XScale', 'log');
title(both, 'Combined');
ylabel(both, 'dB');
xlabel(both, 'Hz');
grid on

x0=100;
y0=200;
width=1366;
height=220;

set(gcf,'units','points','position',[x0,y0,width,height])