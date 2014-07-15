%% Write sinograms from projections generated by using the quarter offset
%% technique.

% Read drasim generated projections and perform:
% - parallel rebinning
% - water beam hardening correction
% - image reconstruction (for display only)
% Save resulting sinograms and drasim generated spectra
%
% (Based on a program by Arif Muhammad.)
% Maria 2012-02-22
% Oscar Grandell 2012-08-27 - Heavy changes
% Robin Westin 2012-10-18 - Updated
% Maria Magnusson 2014-06-11 - Changes to real geometry, better WBHC
% Alexandr Malusek 2014-07-15 - Simplified

% Initialization of projection vectors
% ====================================
%detfanVec = (-(N0-1)/2:(N0-1)/2)*dt0; % no quarter offset
detfanVec = (-(N0-1)/2+1.25:(N0-1)/2+1.25)*dt0; % for swapped detector
betaVec    = -1 * (0:M0-1) * dfi0; % start from 0 going negative...
detparVec  = (-(N1-1)/2:(N1-1)/2)*dt1;
phiVec     = -1 * (0:M1-1) * 180/M1 - gamma*180/pi;

% 90 deg extra for simpler rebinning 
% ----------------------------------
betaVecEXTRA = -1 * (0:(M0*1.25)-1) * dfi0; 

% 180 deg extra for simpler rebinning 
% -----------------------------------
phiVecEXTRA  = -1 * (0:2*M1-1) * 180/M1 - gamma*180/pi; 

% Initialization of mask
% ======================
[x,y] = meshgrid(-(N1-1)/2:(N1-1)/2,-(N1-1)/2:(N1-1)/2);
mask = (x.^2 + y.^2) <= ((N1-1)/2)^2;

% Perform the procedure for both 'Low' and 'High' spectral data
% =============================================================
for i = 1:2
    
    if i == 1
        spect = 'Low';
    else
        spect = 'High';
     end
    
    % Load spectral data and effective attenuation coefficient
    % --------------------------------------------------------
    if strcmp(spect, 'Low');
      load polycr80
      load muEff80
      minv = 19;
      maxv = 25;
      maxvHU = 3000;
    end
    if strcmp(spect, 'High');
      load polycr140Sn
      load muEff140Sn
      minv = 16;
      maxv = 22;
      maxvHU = 2000;
    end

    % Load drasim data
    % ----------------
    if strcmp(spect, 'Low');
        drasimOrig = create_sinogram(0,M0,N0)'; % Create sinogram of data
    elseif strcmp(spect, 'High');
        drasimOrig = create_sinogram(1,M0,N0)'; % Create sinogram of data
    end
    drasimOrig = -log(drasimOrig/max(max(drasimOrig)));

    if i==1
        figure(1)
        imagesc(drasimOrig);
        title('Original drasim Low energy fanbeam sinogram');
        colorbar;
        xlabel('projection angle no');
        ylabel('detector element no');
    end

    % Swap drasim data in the detector direction
    % ------------------------------------------
    drasimOrig = flipud(drasimOrig);

    % Extend fanbeam sinogram with 90 deg
    % -----------------------------------
    drasim = [drasimOrig, drasimOrig(:,1:M0/4)];

    % Rebinning of fanbeam data
    % -------------------------
    [F,t]   = meshgrid(betaVecEXTRA, detfanVec);
    [Fn,tn] = meshgrid(phiVecEXTRA,  detparVec);
    rebsim = interp2(F, t, drasim, Fn-180*asin(tn/L)/pi, asin(tn/L));
    loc = find(isnan(rebsim)==1);   % find NaN:s and replace them with 0
    rebsim(loc) = 0;
    
    % Merge sinogram from 360 to 180 deg
    % ----------------------------------
    rebsim = 0.5 * (rebsim(:,1:M1) + rebsim(end:-1:1,M1+1:2*M1));

    % Copy projection data for later use in DIRA
    % ------------------------------------------
    if strcmp(spect, 'Low');
        projLow = rebsim; 
    elseif strcmp(spect, 'High');
        projHigh  = rebsim;
    end

    % Perform parallel FBP with mask
    % ------------------------------
    recPolysim = iradon(rebsim, phiVec, 'linear', 'Hann', 1, N1)/dt1;
    recPolysim = recPolysim .* mask;

    % Plot data without WBHC
    %-----------------------
    if i==1
        figure(12)
        subplot(221); imagesc(drasim);
        title('drasim data, swapped and extended');colorbar;
        subplot(222); imagesc(rebsim);
        title('rebinned sinogram');colorbar;
        subplot(223); imagesc(recPolysim); axis image
        title('Low energy reconstruction [LAC]');colorbar;
        subplot(224); plot(recPolysim((N1+1)/2,:));
        title('reconstruction, central cut [LAC]');
        axis([1 N1 minv maxv]); grid on;
    elseif i==2
        figure(22)
        subplot(221); imagesc(drasim);
        title('drasim data, swapped and extended');colorbar;
        subplot(222); imagesc(rebsim);
        title('rebinned sinogram');colorbar;
        subplot(223); imagesc(recPolysim); axis image
        title('High energy reconstruction [LAC]');colorbar;
        subplot(224); plot(recPolysim((N1+1)/2,:));
        title('reconstruction, central cut [LAC]');
        axis([1 N1 minv maxv]); grid on;
    end

    % Perform WBHC (obs! now up to k=99)
    %-----------------------------------
    dist = zeros(N1,M1);      
    for  n = 1:M1
      for  m = 1:N1
        value = rebsim(m,n);
        for k = 1:99
          if (value < polycr(k) && k==1)
            dist(m,n) = value / polycr(1); 
            break;    
          else
            if (value>=polycr(k)) && (value<polycr(k+1))
              dist(m,n) = k + (value-polycr(k)) / (polycr(k+1)-polycr(k)); 
              break;
            end
          end
        end
      end
    end 

    BHcorr = muEff * dist ./ (rebsim + eps); 
    rebprojBHcorr = rebsim .* BHcorr;

    % Perform parallel FBP with mask
    % ------------------------------
    recPolysim = iradon(rebprojBHcorr, phiVec, 'linear', 'Hann', 1, N1)/dt1;
    recPolysim = recPolysim .* mask;

    % Plot data with WBHC
    % -------------------
    if i==1
        figure(13)
        subplot(222); imagesc(rebprojBHcorr);
        title('rebinned sinogram + WBHC');colorbar;
        subplot(223); imagesc(recPolysim); axis image
        title('WBHC Low energy reconstruction [LAC]');colorbar;
        subplot(224); plot(recPolysim((N1+1)/2,:));
        title('reconstruction, central cut [LAC]');
        axis([1 N1 minv maxv]); grid on;
    elseif i==2
        figure(23)
        subplot(222); imagesc(rebprojBHcorr);
        title('rebinned sinogram + WBHC');colorbar;
        subplot(223); imagesc(recPolysim); axis image
        title('WBHC High energy reconstruction [LAC]');colorbar;
        subplot(224); plot(recPolysim((N1+1)/2,:));
        title('reconstruction, central cut [LAC]');
        axis([1 N1 minv maxv]); grid on;
    end

    % Use HU-numbers and measure mean and std
    % ---------------------------------------
    recPolyHU = 1000 * (recPolysim  - muEff*100)/(muEff*100);
    if i==1
        figure(14); colormap gray
        hold off
        imagesc(recPolyHU);title('WBHC Low energy reconstruction [HU]');
        colorbar;
        caxis([-150 150])
        hold on
        cx = 387; cy = 137; radius = 20; % Region center and radius
        %[m, sd] = getStdRegion(recPolyHU, cx, cy, radius, 'r')
        cx = 277; cy = 377; radius = 20; % Region center and radius
        %[m, sd] = getStdRegion(recPolyHU, cx, cy, radius, 'g')
    elseif i==2
        figure(24); colormap gray
        hold off
        imagesc(recPolyHU);title('WBHC High energy reconstruction [HU]');
        colorbar;
        caxis([-150 150])
        hold on
        cx = 387; cy = 137; radius = 20; % Region center and radius
        %[m, sd] = getStdRegion(recPolyHU, cx, cy, radius, 'r')
        cx = 277; cy = 377; radius = 20; % Region center and radius
        %[m, sd] = getStdRegion(recPolyHU, cx, cy, radius, 'g')
    end   

    % Copy WBHC projection data for later use in DIRA
    % -----------------------------------------------
    if strcmp(spect, 'Low');
        projLowBH = rebprojBHcorr; 
    elseif strcmp(spect, 'High');
        projHighBH  = rebprojBHcorr;
    end
    
    % Copy Spectra for later use in DIRA
    % ----------------------------------
    fId = fopen(sprintf('spektren/spk1_0.%d.spc', i-1), 'r');
    spectCellTmp = textscan(fId, '%f %f', [2 inf]);
    if strcmp(spect, 'Low');
        currSpectLow(:, 1) = spectCellTmp{1};
        currSpectLow(:, 2) = spectCellTmp{2};
    elseif strcmp(spect, 'High');
        currSpectHigh(:, 1) = spectCellTmp{1};
        currSpectHigh(:, 2) = spectCellTmp{2};
    end
    fclose(fId);
end

save(sinogramsFileName, 'projLow', 'projHigh');
save(sinogramsBhFileName, 'projLowBH', 'projHighBH');
save(spectraFileName, 'currSpectLow', 'currSpectHigh');
