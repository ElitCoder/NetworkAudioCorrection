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
N = 24000;
[Px, fx] = pwelch(x, [], [], N, fsx);
[Py, fy] = pwelch(y, [], [], N, fsy);
% [Px, fx] = pwelch(x, N, N / 2, 'twosided', 'power');
% [Py, fy] = pwelch(y, N, N / 2, 'twosided', 'power');
% 
% Px = Px(1 : N / 2);
% Py = Py(1 : N / 2);
% fx = fx(1 : N / 2);
% fy = fy(1 : N / 2);
% 
% fx = fx * N;
% fy = fy * N;

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

all_index = find(fx >= 43 & fx <= 22721);
fx = fx(all_index);
fy = fy(all_index);
powPx = powPx(all_index);
powPy = powPy(all_index);

min_total = min([min(powPx), min(powPy)]) - 3;
max_total = max([max(powPx), max(powPy)]) + 3;

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

x0=0;
y0=0;
width=1920;
height=300;

set(gcf,'units','points','position',[x0,y0,width,height])
