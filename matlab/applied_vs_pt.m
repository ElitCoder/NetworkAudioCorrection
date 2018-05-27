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

mean_y = mean(powPx) * ones(length(fx), 1);
correction = mean_y - transpose(powPx);

right_axis = abs((max(correction) + 6) - (min(correction) - 6));

% create hardware profile
low = 100;
high = 17000;
steep_low = -24;
steep_high = -24;

profile = 0 * ones(length(fx), 1);

for i = 1:length(fx)
    if fx(i) < low
        steps = log2(low / fx(i));
        attenuation = steps * steep_low;
        
        profile(i) = profile(i) + attenuation;
    elseif fx(i) > high
        steps = log2(fx(i) / high);
        attenuation = steps * steep_high;
        
        profile(i) = profile(i) + attenuation;
    end
end

% combine profile and correction
combined = correction + profile;

% actual calculated eq
eq_freqs = [63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000];
actual_eq = [-11.5543, 5.94052, -6.77812, -0.422895, -0.377122, -1.87099, 0.305217, 2.75767, 12];

%Adds a combined plot of both curves for comparing
both = subplot(1, 1, 1);
%plot(both, fx, correction, 'b');
%hold on
plot(both, eq_freqs, actual_eq, 'b-o');
hold on
plot(both, fx, combined, 'm');
axis(both, [44, 22720, -(right_axis / 2), right_axis / 2]);
%axis tight
set(both, 'XScale', 'log');
title(both, 'Final result vs truncated EQ');
ylabel(both, 'dB');
xlabel(both, 'Hz');
grid on

x0=0;
y0=0;
width=1920;
height=300;

set(gcf,'units','points','position',[x0,y0,width,height])