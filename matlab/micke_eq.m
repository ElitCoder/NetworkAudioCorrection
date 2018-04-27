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
[Px, fx] = pwelch(x, [], [], N, fsx);
[Py, fy] = pwelch(y, [], [], N, fsy);
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

set(0, 'DefaultAxesFontSize', 8);

%Adds a combined plot of both curves for comparing
both = subplot(2, 1, 1);
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
eq_numbers = [63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000];

for i = 1:num
    for j = 1:9
        eq_matrix(i, j) = A((i - 1) * 9 + j);
    end
end

index_vector = zeros(1,9);
for i = 1:9
    [~, index] = min(abs(fx-eq_numbers(i)));
    index_vector(i) = index;
end

eqs = subplot(2, 1, 2);
hold on
for i = 1:num
    if num < 2
        a = eq_matrix(i,:)'; b = num2str(a); c = cellstr(b);
        dx = 0.2; dy = 0.2;
        text(fx(index_vector) + dx, eq_matrix(i,:) + dy, c);
    end
    
    plot(eqs, fx(index_vector), eq_matrix(i,:), '-o');
end

axis(eqs, [44, 22720, -15, 15]);
set(eqs, 'XScale', 'log');
title(eqs, 'EQs');
ylabel(eqs, 'dB');
xlabel(eqs, 'Hz');
grid on