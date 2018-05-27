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

fx = fx(1 : N / 2);
fx = fx * N;

xLin = 0:0.015:log10(length(fx));
for i = 1:length(xLin)
    xLog(i) = round(10^xLin(i));
end

fx = fx(xLog(2:length(xLog)));

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

eqs = subplot(1, 1, 1);

for i = 1:num
    if num < 0
        a = eq_matrix(i,:)'; b = num2str(a); c = cellstr(b);
        dx = 0.2; dy = 0.2;
        text(fx(index_vector) + dx, eq_matrix(i,:) + dy, c);
    end
    
    
    plot(eqs, fx(index_vector), eq_matrix(i,:), '-o');
    hold on
end

axis(eqs, [44, 22720, -15, 15]);
set(eqs, 'XScale', 'log');
title(eqs, 'EQs');
ylabel(eqs, 'dB');
xlabel(eqs, 'Hz');
grid on

x0=0;
y0=0;
width=1920;
height=300;

set(gcf,'units','points','position',[x0,y0,width,height])