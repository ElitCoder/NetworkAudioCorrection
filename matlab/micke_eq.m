clearvars
clear all
close all

[x, fsx] = audioread('before.wav');
[y, fsy] = audioread('after.wav');

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

set(0, 'DefaultAxesFontSize', 8);

ax = subplot(4, 1, 1);
plot(ax,fx, powPx, 'r');
axis(ax,[44, 22720, min_total, max_total]);
set(ax, 'XScale', 'log')
title(ax, 'Before');
ylabel(ax, 'dB');
xlabel(ax, 'Hz');
grid on

ay = subplot(4, 1, 2);
plot(ay, fy, powPy, 'g');
axis(ay, [44, 22720, min_total, max_total]);
set(ay, 'XScale', 'log')
title(ay, 'After');
ylabel(ay, 'dB');
xlabel(ay, 'Hz');
grid on

%Adds a combined plot of both curves for comparing
both = subplot(4, 1, 3);
plot(both, fx, powPx, 'r');
hold on
plot(both, fy, powPy, 'g');
axis(both, [44, 22720, min_total, max_total]);
set(both, 'XScale', 'log');
title(both, 'Combined');
ylabel(both, 'dB');
xlabel(both, 'Hz');
grid on

eq_file = fopen('eqs', 'r');
formatSpec = '%f';
A = fscanf(eq_file, formatSpec);
num = A(1);
A = A(2 : end);

eq_matrix = zeros(num, 9);

for i = 1:num
    for j = 1:9
        eq_matrix(i, j) = A((i - 1) * 9 + j);
    end
end

eqs = subplot(4, 1, 4);
plot(eqs, fx, powPx, 'r');
hold on
plot(eqs, fy, powPy, 'g');
axis(eqs, [44, 22720, -15, 15]);
set(eqs, 'XScale', 'log');
title(eqs, 'EQs');
ylabel(eqs, 'dB');
xlabel(eqs, 'Hz');
grid on