%% Set DIRA's variables
%

setMatlabPath;

% Define what code to use
% 0 = Matlab, 1 = C, 2 = OpenMP
global useCode
useCode = 0;

%% Load data and initialize variables
% ------------------------------------
disp('Loading data and initializing variables...')
tic

% Names of files
sinogramsFileName = 'sinograms.mat';
sinogramsBhFileName = 'sinogramsBH.mat';
spectraFileName = 'spectra.mat';
prostateMaskFileName = 'prostateMask.mat';

%% Scanner model data
smd = ScannerModelData;
smd.eL = 80;              % low x-ray tube voltage in kV
smd.eH = 140;             % high x-ray tube voltage in kV
smd.L = 0.595;            % distance source - rot. center in m
smd.N1 = 511;             % number of detector elements after rebinning
smd.dt1 = 0.402298392584693/(smd.N1+1); % detector element size
smd.interpolation = 2;    % Joseph projection generation
smd.N0 = 512;             % nr of detector elements
smd.M0 = 560;             % nr of projections
smd.fact = 4;             % factor for no rebinned projections
smd.dfi0 = 228 / smd.M0;  % angle increment in degrees
smd.dfi1 = 1 / smd.fact;  % new angle increment in degrees
smd.M1 = 180 * smd.fact;  % new nr of projections
smd.dt0 = (38.4 * pi / 180) / smd.N0;  % Detector element arc length [rad]
smd.gamma = atan(smd.N0 / 2 * smd.dt0 / smd.L);  % First angle after rebinning [rad]

% My phantom model data
pmd = MyPhantomModelData;
pmd.savedIter = [0, 1, 2, 3, 4]; % save data for these iterations
pmd.eEL = 50.0;       % low effective energy in keV
pmd.eEH = 88.5;       % high effective energy in keV
pmd.p2MD = 1;         % using 2MD.
pmd.p3MD = 1;         % using 3MS.
pmd.recAlg = 0;       % reconstruction algorithm (0 = old DIRA, 1 = iterative DEFBP)
pmd.atlasImage = imread('atlasImage_slice113.png');
pmd.referenceImage = imread('referenceImage_slice113.png');

% The setting of material data takes long time. Skip it if not needed.
if ~pSetMaterialData
  return
end

spectra =  load(spectraFileName);
smd.ELow = spectra.currSpectLow(1:75, 1);
smd.NLow = spectra.currSpectLow(1:75, 2);
smd.EHigh = spectra.currSpectHigh(1:135, 1);
smd.NHigh = spectra.currSpectHigh(1:135, 2);

sinograms = load(sinogramsFileName);
pmd.projLow = sinograms.projLow;
pmd.projHigh = sinograms.projHigh;

sinogramsBH = load(sinogramsBhFileName);
pmd.projLowBH = sinogramsBH.projLowBH;
pmd.projHighBH = sinogramsBH.projHighBH;

%% Elemental material composition (number of atoms per molecule) and
% mass density (in g/cm^3).

% Compact bone
boneStr = 'H0.403076C0.149395N0.033840O0.316004Na0.001473Mg0.000929P0.034249S0.001056Ca0.059978';
boneDens = 1.920;

% Bone marrow mixture
marrowMixStr = 'H0.620994C0.250615N0.008329O0.119144Na0.000124P0.000092S0.000267Cl0.000240K0.000145Fe0.000051';
marrowMixDens = 1.005;

% Lipid
lipidStr = 'H0.621918C0.341890O0.036192';
lipidDens = 0.920;

% Proteine
proteineStr = 'H0.480981C0.326573N0.089152O0.101004S0.002291';
proteineDens = 1.350;

% Water
waterStr = 'H0.666667O0.333333';
waterDens = 1.000;

% Doublets for MD2
pmd.name2{1}{1} = 'compact bone';
pmd.Dens2{1}(1) = boneDens;
Cross2{1}(:, 1) = [CalculateMAC(boneStr, pmd.eEL),...
  CalculateMAC(boneStr, pmd.eEH)];
pmd.Att2{1}(:, 1) = pmd.Dens2{1}(1) * Cross2{1}(:, 1);
mu2Low{1}(:, 1) = CalculateMACs(boneStr, 1:smd.eL);
mu2High{1}(:, 1) = CalculateMACs(boneStr, 1:smd.eH);

pmd.name2{1}{2} = 'bone marrow';
pmd.Dens2{1}(2) = marrowMixDens;
Cross2{1}(:, 2) = [CalculateMAC(marrowMixStr, pmd.eEL),...
  CalculateMAC(marrowMixStr, pmd.eEH)];
pmd.Att2{1}(:, 2) = pmd.Dens2{1}(2) * Cross2{1}(:, 2);
mu2Low{1}(:, 2) = CalculateMACs(marrowMixStr, 1:smd.eL);
mu2High{1}(:, 2) = CalculateMACs(marrowMixStr, 1:smd.eH);

% Triplets for MD3
pmd.name3{1}{1} = 'lipid';
pmd.Dens3{1}(1) = lipidDens;
pmd.Att3{1}(:, 1) = [pmd.Dens3{1}(1) * CalculateMAC(lipidStr, pmd.eEL),...
  pmd.Dens3{1}(1) * CalculateMAC(lipidStr, pmd.eEH)];
mu3Low{1}(:, 1) = pmd.Dens3{1}(1) * CalculateMACs(lipidStr, 1:smd.eL);
mu3High{1}(:, 1) = pmd.Dens3{1}(1) * CalculateMACs(lipidStr, 1:smd.eH);

pmd.name3{1}{2} = 'proteine';
pmd.Dens3{1}(2) = proteineDens;
pmd.Att3{1}(:, 2) = [pmd.Dens3{1}(2) * CalculateMAC(proteineStr, pmd.eEL),...
  pmd.Dens3{1}(2) * CalculateMAC(proteineStr, pmd.eEH)];
mu3Low{1}(:, 2) = pmd.Dens3{1}(2) * CalculateMACs(proteineStr, 1:smd.eL);
mu3High{1}(:, 2) = pmd.Dens3{1}(2) * CalculateMACs(proteineStr, 1:smd.eH);

pmd.name3{1}{3} = 'water';
pmd.Dens3{1}(3) = waterDens;
pmd.Att3{1}(:, 3) = [pmd.Dens3{1}(3) * CalculateMAC(waterStr, pmd.eEL),...
  pmd.Dens3{1}(3) * CalculateMAC(waterStr, pmd.eEH)];
mu3Low{1}(:, 3) = pmd.Dens3{1}(3) * CalculateMACs(waterStr, 1:smd.eL);
mu3High{1}(:, 3) = pmd.Dens3{1}(3) * CalculateMACs(waterStr, 1:smd.eH);

pmd.muLow = cat(2, mu2Low{1}, mu3Low{1});
pmd.muHigh = cat(2, mu2High{1}, mu3High{1});
