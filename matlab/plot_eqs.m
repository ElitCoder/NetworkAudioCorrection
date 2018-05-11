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

x0=100;
y0=200;
width=1920;
height=300;

set(gcf,'units','points','position',[x0,y0,width,height])