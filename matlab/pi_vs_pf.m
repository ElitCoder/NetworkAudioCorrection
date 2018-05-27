clearvars
close all

[x, fsx] = audioread('before.wav');

sound_start_sec = 2;
sound_stop_sec = 30;

% cut silence
x = x(fsx * sound_start_sec : fsx * sound_stop_sec);

% create spectrum
N = 8192;
[Px, fx] = pwelch(x, N, N / 2, 'twosided', 'power');

Px = Px(1 : N / 2);
fx = fx(1 : N / 2);
fx = fx * N;

powPx = pow2db(Px);

xLin = 0:0.015:log10(length(fx));
for i = 1:length(xLin)
    xLog(i) = round(10^xLin(i));
end

for i = 2:length(xLog)-1
   newPowPx(i) = mean(powPx(xLog(i-1):xLog(i+1))); 
end

fx = fx(xLog(2:length(xLog)));
powPx = newPowPx;

all_index = find(fx >= 43 & fx <= 22721);

fx = fx(all_index);
powPx = powPx(all_index);

min_total = min(powPx) - 3;
max_total = max(powPx) + 3;

mean_y = mean(powPx) * ones(length(fx), 1);

%Adds a combined plot of both curves for comparing
both = subplot(1, 1, 1);
plot(both, fx, powPx, 'r');
hold on
plot(both, fx, mean_y, 'g');
axis(both, [44, 22720, min_total, max_total]);
set(both, 'XScale', 'log');
title(both, 'Actual spectrum vs desired spectrum');
ylabel(both, 'dBFS');
xlabel(both, 'Hz');
grid on

x0=0;
y0=0;
width=1920;
height=300;

set(gcf,'units','points','position',[x0,y0,width,height])