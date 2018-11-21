% t = 0:1/48000:10;
% fo = 20;
% f1 = 20000;
% y = chirp(t,fo,10,f1,'logarithmic');
% fsy = 48000;

clearvars
close all

[x, fsx] = audioread('before.wav');
[y, fsy] = audioread('after.wav');

sound_start_sec = 3;
sound_stop_sec = 30;

x = x(fsx * sound_start_sec : fsx * sound_stop_sec);
y = y(fsy * sound_start_sec : fsy * sound_stop_sec);

X = abs(fft(x));
Y = abs(fft(y));
X = X(1:round(length(x)/2+1));
Y = Y(1:round(length(y)/2+1));

fX = fsx*(0:(length(x)/2))/length(x);
fY = fsy*(0:(length(y)/2))/length(y);

for i=1:length(fX)
    X(i) = X(i) * sqrt(fX(i));
end

for i=1:length(fY)
    Y(i) = Y(i) * sqrt(fY(i));
end

xLinX = 0:0.015:log10(length(fX));
for i = 1:length(xLinX)
    xLogX(i) = round(10^xLinX(i));
end

xLinY = 0:0.015:log10(length(fY));
for i = 1:length(xLinY)
    xLogY(i) = round(10^xLinY(i));
end

for i = 2:length(xLogX)-1
   newPowPx(i) = mean(X(xLogX(i-1):xLogX(i+1)));
end

for i = 2:length(xLogY)-1
   newPowPy(i) = mean(Y(xLogY(i-1):xLogY(i+1)));
end

fX = fX(xLogX(2:length(xLogX)));
fY = fY(xLogY(2:length(xLogY)));

X = newPowPx;
Y = newPowPy;

X = mag2db(X);
Y = mag2db(Y);

semilogx(fX, X, 'r')
hold on
semilogx(fY, Y, 'g')
grid on
axis([20 20000 -inf inf])