#include <string>
#include <fstream>
#include <vector>
#include <map>
#include <algorithm>
#include <stdint.h>
#include <cstdlib> // atof
#include <cstring>
#include <stdint.h>
#include <getopt.h>

#include <cstdlib> // atof
#include <cmath>
#include <iomanip>
#include <complex>
#include <fftw3.h>

#include "err/err.h"
#include "fft/fft.h"
#include "fit_signal/fit_signal.h"
#include "dimage/dimage.h"
#include "str2vec/str2vec.h"
#include "signal/signal.h"

#define FTYPE double

using namespace std;



/******************************************************************/
void
flt_pnm(ostream & ff, const Signal & s, const int argc, char **argv){
  const char *name = "pnm";

  int W=1024, H=768;
  // parse options (opterr==0, optint==1)
  while(1){
    int c = getopt(argc, argv, "+W:H:");
    if (c==-1) break;
    switch (c){
      case '?': throw Err() << name << ": unknown option: -" << (char)optopt;
      case ':': throw Err() << name << ": no argument: -" << (char)optopt;
      case 'W': W = atoi(optarg); break;
      case 'H': H = atoi(optarg); break;
    }
  }
  if (argc-optind>0) throw Err() << name << ": extra argument found: " << argv[0];


  int colors[] = {0x00880000, 0x00008800, 0x00000088,
                  0x00888800, 0x00008888, 0x00880088,
                  0x00000000};
  int cn = sizeof(colors)/sizeof(int); // number of colors

  // the picture
  vector<int> pic(W*H, (int)0xFFFFFFFF);
  int N = s.get_n();
  int cN  = s.get_ch();
  if (N<1 || cN<1) return;

  // plot t=0 line
  int x0 = (-s.t0/s.dt*W)/N; 
  if (x0>=0 && x0<W) for (int y=0;y<H;y++){ pic[y*W+x0] = 0x00888888;}
  //// plot y=0 line
  // int y0 = H/2;
  // for (int x=0;x<W;x++){ pic[y0*W+x] = 0x00888888;}

  for (int ch = 0; ch<s.get_ch(); ch++){
    for (int i=0; i<N; i++){
      int x = ((double)i*W)/N; // be worry about int overfull
      int y = H/2 - (s.chan[ch][i]*H)/(1<<16);
      if (y<0 || y>=H || x<0 || x>=W) continue;
      pic[y*W+x] = colors[ch % cn];
    }
  }
  ff << "P6\n" << W << " " << H << "\n255\n";
  for (int y=0; y<H; y++){
    for (int x=0; x<W; x++){
      ff.write((char *)(&pic[y*W+x])+1, 3);
    }
  }
}


/******************************************************************/

void
flt_fft_txt(ostream & ff, const Signal & s, const int argc, char **argv){
  const char *name = "fft_txt";

  double fmin=0, fmax=+HUGE_VAL;
  bool use_bl = false; //use Blackman window
  // parse options (opterr==0, optint==1)
  while(1){
    int c = getopt(argc, argv, "+F:G:B");
    if (c==-1) break;
    switch (c){
      case '?': throw Err() << name << ": unknown option: -" << (char)optopt;
      case ':': throw Err() << name << ": no argument: -" << (char)optopt;
      case 'F': fmin = atof(optarg); break;
      case 'G': fmax = atof(optarg); break;
      case 'B': use_bl = true; break;
    }
  }
  if (argc-optind>0) throw Err() << name << ": extra argument found: " << argv[0];


  int N = s.get_n();
  int cN  = s.get_ch();
  if (N<1 || cN<1) return;

  FFT fft(N);
  int i1f, i2f;
  double df;
  fft.get_ind(s.dt, &fmin, &fmax, &i1f, &i2f, &df);
  vector<vector<double> > dat_im(cN), dat_re(cN);
  for (int c = 0; c<cN; c++){
    fft.run(s.chan[c].data(), s.chan[c].sc, use_bl);
    dat_re[c] = fft.real(i1f,i2f);
    dat_im[c] = fft.imag(i1f,i2f);
  }

  ff << scientific;
  for (int i=i1f; i<i2f; i++){
    ff << setprecision(12) << df*i;
    for (int c = 0; c<cN; c++){
      ff << setprecision(8) 
         << "\t" << dat_re[c][i-i1f]
         << "\t" << dat_im[c][i-i1f];
    }
    ff << "\n";
  }
}


/******************************************************************/

void
flt_fft_pow(ostream & ff, const Signal & s, const int argc, char **argv){
  const char *name = "fft_pow";

  double fmin = 0, fmax=+HUGE_VAL;
  double npts = 1024;
  int    log  = 0;
  bool   use_bl = false; // use blackman window

  // parse options (opterr==0, optint==1)
  while(1){
    int c = getopt(argc, argv, "+F:G:N:lB");
    if (c==-1) break;
    switch (c){
      case '?': throw Err() << name << ": unknown option: -" << (char)optopt;
      case ':': throw Err() << name << ": no argument: -" << (char)optopt;
      case 'F': fmin = atof(optarg); break;
      case 'G': fmax = atof(optarg); break;
      case 'N': npts = atof(optarg); break;
      case 'B': use_bl = true; break;
      case 'l': log = 1; break;
    }
  }
  if (argc-optind>0) throw Err() << name << ": extra argument found: " << argv[0];

  int N = s.get_n();
  int cN  = s.get_ch();
  if (N<1 || cN<1) return;

  double k = 2*s.dt/N; // convert power to V^2/Hz

  FFT fft(N);
  if (use_bl) k*=fft.blackman_corr();

  int i1f, i2f;
  double df;
  fft.get_ind(s.dt, &fmin, &fmax, &i1f, &i2f, &df);
  vector<vector<double> > dat(cN);
  for (int c = 0; c<cN; c++){
    fft.run(s.chan[c].data(), s.chan[c].sc, use_bl);
    dat[c] = fft.abs(i1f,i2f);
  }

  // log scale
  if (log) {
    if (fmin==0) fmin=df;
    double fstep = pow(fmax/fmin, 1.0/npts);

    ff << scientific;
    vector<double> ss(cN, 0);
    int n = 0; // number of samples in the average
    for (int i=i1f; i<i2f; i++){
      for (int c = 0; c<cN; c++) ss[c]+=pow(dat[c][i-i1f],2);
      n++;
      // print point and reset counters if needed
      if (i*df >= fmin*fstep || i==i2f-1){
        ff << setprecision(10) << (i-0.5*(n-1))*df;
        for (int c = 0; c<cN; c++){
          ff << "\t" << setprecision(8) << k*ss[c]/n;
          ss[c]=0;
        }
        ff << "\n";
        n=0; fmin=i*df;
      }
    }
  }

  // linear scale
  else {
    double fstep = (fmax-fmin)/npts;

    ff << scientific;
    vector<double> ss(cN, 0);
    int n = 0; // number of samples in the average
    for (int i=i1f; i<i2f; i++){
      for (int c = 0; c<cN; c++) ss[c]+=pow(dat[c][i-i1f],2);
      n++;
      // print point and reset counters if needed
      if (i*df >= fmin + fstep || i==i2f-1){
        ff << setprecision(10) << (i-0.5*(n-1))*df;
        for (int c = 0; c<cN; c++){
          ff << "\t" << setprecision(8) << k*ss[c]/n;
          ss[c]=0;
        }
        ff << "\n";
        n=0; fmin=i*df;
      }
    }
  }

}

/******************************************************************/

void
flt_fft_pow_corr(ostream & ff, const Signal & s, const int argc, char **argv){
  const char *name = "fft_pow_corr";

  double fmin = 0, fmax=+HUGE_VAL;
  double npts = 1024;
  int    log  = 0;
  bool use_bl = false; // use Blackman window
  // parse options (opterr==0, optint==1)
  while(1){
    int c = getopt(argc, argv, "+F:G:N:lB");
    if (c==-1) break;
    switch (c){
      case '?': throw Err() << name << ": unknown option: -" << (char)optopt;
      case ':': throw Err() << name << ": no argument: -" << (char)optopt;
      case 'F': fmin = atof(optarg); break;
      case 'G': fmax = atof(optarg); break;
      case 'N': npts = atof(optarg); break;
      case 'l': log = 1; break;
      case 'B': use_bl = true; break;
    }
  }
  if (argc-optind>0) throw Err() << name << ": extra argument found: " << argv[0];


  int N = s.get_n();
  int cN  = s.get_ch();
  // we need two channels!
  if (N<2 || cN!=2) return;

  double k = 2*s.dt/N; // convert power to V^2/Hz

  FFT fft(N);
  if (use_bl) k*=fft.blackman_corr();


  int i1f, i2f;
  double df;
  fft.get_ind(s.dt, &fmin, &fmax, &i1f, &i2f, &df);
  vector<double> dat1re, dat1im, dat2re, dat2im, dat0re, dat0im;

  // run fft on the first channel
  fft.run(s.chan[0].data(), s.chan[0].sc, use_bl);
  // copy complex values
  dat1re = fft.real(i1f,i2f);
  dat1im = fft.imag(i1f,i2f);
  fft.run(s.chan[1].data(), s.chan[1].sc, use_bl);
  dat2re = fft.real(i1f,i2f);
  dat2im = fft.imag(i1f,i2f);

  // correlation is a A*B^*
  // http://mathworld.wolfram.com/Cross-CorrelationTheorem.html
  for (int i=0; i<dat1re.size(); i++){
    dat0re.push_back(dat1re[i]*dat2re[i]+dat1im[i]*dat2im[i]);
    dat0im.push_back(dat1im[i]*dat2re[i]-dat1re[i]*dat2im[i]);
  }

  // log scale
  if (log) {
    if (fmin==0) fmin=df;
    double fstep = pow(fmax/fmin, 1.0/npts);

    ff << scientific;
    double sre=0, sim=0;
    int n = 0; // number of samples in the average
    for (int i=i1f; i<i2f; i++){
      sre+=dat0re[i-i1f];
      sim+=dat0im[i-i1f];
      n++;
      // print point and reset counters if needed
      if (i*df >= fmin*fstep || i==i2f-1){
        ff << setprecision(10) << (i-0.5*(n-1))*df << "\t" << setprecision(8) << k*hypot(sre,sim)/n << "\n";
        sre=0; sim=0;
        n=0; fmin=i*df;
      }
    }
  }

  // linear scale
  else {
    double fstep = (fmax-fmin)/npts;

    ff << scientific;
    double sre=0, sim=0;
    int n = 0; // number of samples in the average
    for (int i=i1f; i<i2f; i++){
      sre+=dat0re[i-i1f];
      sim+=dat0im[i-i1f];
      n++;
      // print point and reset counters if needed
      if (i*df >= fmin + fstep || i==i2f-1){
        ff << setprecision(10) << (i-0.5*(n-1))*df << "\t" << setprecision(8) << k*hypot(sre,sim)/n << "\n";
        sre=0; sim=0;
        n=0; fmin=i*df;
      }
    }
  }
}

/******************************************************************/

void
flt_sfft_txt(ostream & ff, const Signal & s, const int argc, char **argv) {
  const char *name = "sfft_txt";

  double fmin=0, fmax=+HUGE_VAL;
  int win = 1024;
  // parse options (opterr==0, optint==1)
  while(1){
    int c = getopt(argc, argv, "+F:G:w:");
    if (c==-1) break;
    switch (c){
      case '?': throw Err() << name << ": unknown option: -" << (char)optopt;
      case ':': throw Err() << name << ": no argument: -" << (char)optopt;
      case 'F': fmin = atof(optarg); break;
      case 'G': fmax = atof(optarg); break;
      case 'w': win  = atof(optarg); break;
    }
  }
  if (argc-optind>0) throw Err() << name << ": extra argument found: " << argv[0];

  int N = s.get_n();
  int cN  = s.get_ch();
  if (N<1 || cN<1) return;
  int ch=0;

  FFT fft(win);
  int i1f, i2f;
  double df;
  fft.get_ind(s.dt, &fmin, &fmax, &i1f, &i2f, &df);

  for (int iw=0; iw<N-win; iw+=win){
    fft.run(s.chan[ch].data()+iw, s.chan[ch].sc, true);

    // print selected frequency range
    ff << scientific;
    for (int i=i1f; i<i2f; i++){
      ff << setprecision(10) << s.t0 + s.dt*(iw+win/2) << "\t" << i*df << "\t"
         << setprecision(8) << fft.real(i) << "\t" << fft.imag(i) << "\n";
    }
    ff << "\n";
  }
}

/******************************************************************/
void
flt_sfft_pow(ostream & ff, const Signal & s, const int argc, char **argv) {
  const char *name = "sfft_pow";

  double fmin=0, fmax=+HUGE_VAL;
  int win = 1024;
  bool avr = false;
  // parse options (opterr==0, optint==1)
  while(1){
    int c = getopt(argc, argv, "+F:G:w:a");
    if (c==-1) break;
    switch (c){
      case '?': throw Err() << name << ": unknown option: -" << (char)optopt;
      case ':': throw Err() << name << ": no argument: -" << (char)optopt;
      case 'F': fmin = atof(optarg); break;
      case 'G': fmax = atof(optarg); break;
      case 'w': win  = atof(optarg); break;
      case 'a': avr  = true; break;
    }
  }
  if (argc-optind>0) throw Err() << name << ": extra argument found: " << argv[0];

  int N = s.get_n();
  int cN  = s.get_ch();
  if (N<1 || cN<1) return;
  int maxch = avr? cN:1;

  FFT fft(win);
  int i1f, i2f;
  double df;
  fft.get_ind(s.dt, &fmin, &fmax, &i1f, &i2f, &df);

  for (int iw=0; iw<N-win; iw+=win){
    std::vector<double> data(i2f-i1f, 0);
    for (int ch = 0; ch<maxch; ch++) {
      fft.run(s.chan[ch].data()+iw, s.chan[ch].sc, true);
      for (int j = i1f; j<i2f; j++) data[j-i1f] += fft.abs(j)/maxch;
    }

    // print selected frequency range
    ff << scientific;
    for (int j=i1f; j<i2f; j++){
      ff << setprecision(10) << s.t0 + s.dt*(iw+win/2) << "\t" << j*df << "\t"
         << setprecision(8) << data[j-i1f] << "\n";
    }
    ff << "\n";
  }
}


/******************************************************************/

void
flt_sfft_int(ostream & ff, const Signal & s, const int argc, char **argv) {
  const char *name = "sfft_int";

  double fmin=0, fmax=+HUGE_VAL;
  int win = 1024;
  // parse options (opterr==0, optint==1)
  while(1){
    int c = getopt(argc, argv, "+F:G:w:");
    if (c==-1) break;
    switch (c){
      case '?': throw Err() << name << ": unknown option: -" << (char)optopt;
      case ':': throw Err() << name << ": no argument: -" << (char)optopt;
      case 'F': fmin = atof(optarg); break;
      case 'G': fmax = atof(optarg); break;
      case 'w': win  = atof(optarg); break;
    }
  }
  if (argc-optind>0) throw Err() << name << ": extra argument found: " << argv[0];

  int N = s.get_n();
  int cN  = s.get_ch();
  if (N<1 || cN<1) return;
  int ch=0;

  FFT fft(win);
  int i1f, i2f;
  double df;
  fft.get_ind(s.dt, &fmin, &fmax, &i1f, &i2f, &df);

  for (int iw=0; iw<N-win; iw+=win){
    fft.run(s.chan[ch].data()+iw, s.chan[ch].sc, true);

    // calculate integral over the selected frequency range
    ff << scientific;
    double sum=0.0;
    int n=0;
    for (int i=i1f; i<i2f; i++){
      sum+=fft.abs(i)*fft.abs(i);
      n++;
    }
    ff << setprecision(10) << s.t0 + s.dt*(iw+win/2) << "\t" << setprecision(8) << sqrt(sum/n) << "\n";
  }
}

/******************************************************************/

void
flt_sfft_diff(ostream & ff, const Signal & s, const int argc, char **argv) {
  const char *name = "sfft_diff";

  double fmin=0, fmax=+HUGE_VAL;
  int win = 1024;
  // parse options (opterr==0, optint==1)
  while(1){
    int c = getopt(argc, argv, "+F:G:w:");
    if (c==-1) break;
    switch (c){
      case '?': throw Err() << name << ": unknown option: -" << (char)optopt;
      case ':': throw Err() << name << ": no argument: -" << (char)optopt;
      case 'F': fmin = atof(optarg); break;
      case 'G': fmax = atof(optarg); break;
      case 'w': win  = atof(optarg); break;
    }
  }
  if (argc-optind>0) throw Err() << name << ": extra argument found: " << argv[0];
  int N = s.get_n();
  int cN  = s.get_ch();
  if (N<1 || cN<1) return;
  int ch=0;

  FFT fft(win);
  int i1f, i2f;
  double df;
  fft.get_ind(s.dt, &fmin, &fmax, &i1f, &i2f, &df);

  // array for previouse line
  vector<double> p(i2f-i1f, 0.0);

  for (int iw=0; iw<N-win; iw+=win){
    fft.run(s.chan[ch].data()+iw, s.chan[ch].sc, true);
    if (iw!=0){
      // calculate distance between this line and previous line
      double dist=0; int distn=0;
      for (int i=i1f; i<i2f; i++){
        dist += pow(p[i-i1f] - fft.abs(i), 2); distn++;
      }
      ff << s.t0 + s.dt*(iw+win/2) << '\t' << dist/distn << '\n';
    }
    // save previous line
    for (int i=i1f; i<i2f; i++) p[i-i1f] = fft.abs(i);
  }
}


/******************************************************************/

void
flt_sfft_peaks(ostream & ff, const Signal & s, const int argc, char **argv) {
  const char *name = "sfft_peaks";

  double fmin=0, fmax=+HUGE_VAL;
  int win = 1024;
  double th = 2.5;
  bool avr = false;
  // parse options (opterr==0, optint==1)
  while(1){
    int c = getopt(argc, argv, "+F:G:w:t:a");
    if (c==-1) break;
    switch (c){
      case '?': throw Err() << name << ": unknown option: -" << (char)optopt;
      case ':': throw Err() << name << ": no argument: -" << (char)optopt;
      case 'F': fmin = atof(optarg); break;
      case 'G': fmax = atof(optarg); break;
      case 'w': win  = atof(optarg); break;
      case 't': th   = atof(optarg); break;
      case 'a': avr  = true; break;
    }
  }
  if (argc-optind>0) throw Err() << name << ": extra argument found: " << argv[0];


  int N = s.get_n();
  int cN  = s.get_ch();
  if (N<1 || cN<1) return;
  int maxch = avr? cN:1;

  FFT fft(win);
  int i1f, i2f;
  double df;
  fft.get_ind(s.dt, &fmin, &fmax, &i1f, &i2f, &df);

  // for each time bin do fft and find peaks
  for (int iw=0; iw<N-win; iw+=win){

    std::vector<double> data(i2f-i1f, 0);
    for (int ch = 0; ch<maxch; ch++) {
      fft.run(s.chan[ch].data()+iw, s.chan[ch].sc, true);
      for (int j = i1f; j<i2f; j++) data[j-i1f] += fft.abs(j)/maxch;
    }

    // array for peak "sizes"
    vector<double> e(i2f-i1f, 0.0);

    // for all peak widths from 1 to 20
    for (int W=1; W<20; W++) {
      if (e.size()<=2*W) continue; // too few points

      // for each peak position
      for (int i=i1f+W; i<i2f-W-1; i++){
        double x0 = i*df;
        double y0 = data[i-i1f];
        double sum = 0.0;
        int sumn = 0;
        for (int j=-W; j<=W; j++) {
          double x = (i+j)*df;
          double y = data[i+j-i1f]/y0;
          // model peak with width 4*W, f(x)
          // (we will look only at the central part, -W:W)
          double f = 1/(pow(x-x0,2) + pow(4*W,2));
          // find mean square difference
          sum+=pow(y-f,2); sumn++;
        }
        sum=sqrt(sum/sumn);
        // find the best width
        if (e[i-i1f] < 1/sum) e[i-i1f] = 1/sum;
      }
    }

    // find mean value for e vector
    double eavr=0.0; int eavrn=0;
    for (int i = i1f; i<i2f; i++){
      eavr+=e[i-i1f]; eavrn++;
    }
    eavr/=eavrn;

    // now select peaks which are above threshold
    ff << s.t0 + s.dt*(iw+win/2);
    for (int i = i1f+1; i<i2f-1; i++){
      double v = e[i-i1f]/eavr;
      double vp = e[i-i1f+1]/eavr;
      double vn = e[i-i1f-1]/eavr;
      if (v>th && v>vn && v>vp){
        ff << "\t" << setprecision(10) << i*df << " " << setprecision(8) << data[i-i1f];
      }
    }
    ff << "\n";
  }
}

/******************************************************************/

void
flt_sfft_peak(ostream & ff, const Signal & s, const int argc, char **argv) {
  const char *name = "sfft_peak";

  vector<double> tvec, fvec;
  int win = 1024;
  int stp = 0;
  double fwin = 0;
  // parse options (opterr==0, optint==1)
  while(1){
    int c = getopt(argc, argv, "+F:T:w:f:s:");
    if (c==-1) break;
    switch (c){
      case '?': throw Err() << name << ": unknown option: -" << (char)optopt;
      case ':': throw Err() << name << ": no argument: -" << (char)optopt;
      case 'F': fvec = str2dvec(optarg); break;
      case 'T': tvec = str2dvec(optarg); break;
      case 'w': win  = atof(optarg); break;
      case 's': stp  = atof(optarg); break;
      case 'f': fwin = atof(optarg); break;
    }
  }
  if (argc-optind>0) throw Err() << name << ": extra argument found: " << argv[0];

  if (fwin==0) fwin = 20.0/(win*s.dt);
  if (stp==0) stp = win;

  // Put fvec and tvec into a vector of double pairs, sort it
  vector<pair<double,double> > hints;
  vector<double>::const_iterator it1,it2;
  for (it1=tvec.begin(),  it2=fvec.begin();
       it1!=tvec.end() && it2!=fvec.end(); it1++, it2++){
    hints.push_back(make_pair(*it1, *it2));
  }
  sort(hints.begin(), hints.end());
  if (hints.size()<2) throw Err() << name
    << ": lists of times and frequencies are expected (-T and -F options)";

  int N = s.get_n();
  int cN  = s.get_ch();
  if (N<1 || cN<1) return;
  int ch=0;

  // find time indices
  int i1 = ceil((hints.begin()->first - s.t0) / s.dt - win/2.0);
  int i2 = floor((hints.rbegin()->first - s.t0) / s.dt + win/2.0);
  if (i1<0) i1=0;
  if (i2>=N) i2=N-1;
  if (i2-i1 < win) throw Err() << name
    << ": too short time range";

  // find min/max freq
  double fmin = *min_element(fvec.begin(), fvec.end()) - fwin/2;
  double fmax = *max_element(fvec.begin(), fvec.end()) + fwin/2;
  if (fmin<=0) throw Err() << name << ": negative or zero frequency";


  FFT fft(win);
  int i1f, i2f;
  double df;
  fft.get_ind(s.dt, &fmin, &fmax, &i1f, &i2f, &df);

  // for each time bin do fft and find peaks
  for (int iw=i1; iw<i2-win; iw+=stp){
    fft.run(s.chan[ch].data()+iw, s.chan[ch].sc, true);

    // get frequency for this time (linear interpolation of hints array)
    double t0 = s.t0 + s.dt*(iw+win/2);
    double f0 = 0;
    vector<pair<double,double> >::const_iterator it;
    for (it=hints.begin(); it!=hints.end()-1; it++){
      if (it->first > t0 || (it+1)->first <= t0) continue;
      f0 = it->second + ((it+1)->second - it->second)/
                        ((it+1)->first - it->first)*(t0-it->first);
    }
    if (f0==0) throw Err() << name << ": can't get frequency for t=" << t0;

    // frequency indices for the window around f0
    int iw1f = (f0-fwin/2)/df;
    int iw2f = (f0+fwin/2)/df;
    if (iw1f<i1f) iw1f=i1f;
    if (iw2f>=i2f) iw2f=i2f;
    // find peak position (just a maximum)
    double pind = iw1f;
    for (int i=iw1f+1; i<iw2f-1; i++){
      if (fft.abs(pind) < fft.abs(i)) pind=i;
    }

    double x1=(pind-1)*df, x2=pind*df, x3=(pind+1)*df;
    double y1=fft.abs(pind-1), y2=fft.abs(pind), y3=fft.abs(pind+1);

    //3-pt quadratic fit
    double D = x1*x1*(x2-x3) + x3*x3*(x1-x2) + x2*x2*(x3-x1);
    if (D==0) continue;
    double A = (y1*(x2-x3) + y3*(x1-x2) + y2*(x3-x1)) / D;
    double B = (x1*x1*(y2-y3) + x3*x3*(y1-y2) + x2*x2*(y3-y1)) / D;
    double C = (x1*x1*(x2*y3-x3*y2) + x3*x3*(x1*y2-x2*y1) + x2*x2*(x3*y1-x1*y3)) / D;

    double x0 = -B/(2*A);
    double y0 = A*x0*x0 + B*x0 + C;

    if (x0<x1 || x0>x3) {x0=x2; y0=y2;}

    ff << t0 << " " << setprecision(10) << x0 << " " << setprecision(8) << (y1+y2+y3)/3 << " "
       << (fft.abs(iw1f)+fft.abs(iw2f))/2 << "\n";
  }

}


/******************************************************************/

void
flt_sfft_pnm(ostream & ff, const Signal & s, const int argc, char **argv) {
  const char *name = "sfft_pnm";

  double fmin=0, fmax=+HUGE_VAL;
  double amin=-HUGE_VAL, amax=+HUGE_VAL;
  int win = 0;
  int W=1024, H=768, S=16;
  bool l = false;
  const char *g = "KRYW";
  bool avr = false;
  // parse options (opterr==0, optint==1)
  while(1){
    int c = getopt(argc, argv, "+F:G:A:B:W:H:S:w:lg:a");
    if (c==-1) break;
    switch (c){
      case '?': throw Err() << name << ": unknown option: -" << (char)optopt;
      case ':': throw Err() << name << ": no argument: -" << (char)optopt;
      case 'F': fmin = atof(optarg); break;
      case 'G': fmax = atof(optarg); break;
      case 'A': amin = atof(optarg); break;
      case 'B': amax = atof(optarg); break;
      case 'W': W    = atoi(optarg); break;
      case 'H': H    = atoi(optarg); break;
      case 'S': S    = atoi(optarg); break;
      case 'w': win  = atof(optarg); break;
      case 'l': l    = true; break;
      case 'g': g    = optarg; break;
      case 'a': avr  = true; break;
    }
  }
  if (argc-optind>0) throw Err() << name << ": extra argument found: " << argv[0];

  int N = s.get_n();
  int cN  = s.get_ch();
  if (N<1 || cN<1) return;

  // choose default window in such a way that t and f resolution (Dt and Df) are same:
  //   Dt = s.dt*win = 2*pi/Df;
  //   H/W * N * 2*pi/(fmax-fmin)/s.dt) = win^2;
  // =>  win^2 = N^2 H/W * 2*pi/((fmax-fmin)*s.dt*N).
  if (win<=0){
    // same as in fft.get_ind
    if (fmax<fmin) std::swap(fmax, fmin);
    if (fmax > 0.5/s.dt) fmax = 0.5/s.dt;
    if (fmin > 0.5/s.dt) fmin = 0.5/s.dt;
    if (fmin < 0) fmin = 0;
    win = N*sqrt(2*M_PI*(double)H/W/((fmax-fmin)*s.dt*N));
    cerr << "win: " << win << "\n";
  }

  FFT fft(win);
  int i1f, i2f;
  double df;
  fft.get_ind(s.dt, &fmin, &fmax, &i1f, &i2f, &df);

  dImage pic(W,H,0, amin, amax);
  pic.set_vs(S);

  int nc = avr?s.get_ch():1;
  for (int ch = 0; ch < nc; ch++){
    for (int x = 0; x<W; x++){
      int il = ((double)(N-win)*x)/(W-1); // be worry about int overfull
      fft.run(s.chan[ch].data()+il, s.chan[ch].sc, true);
      for (int y = 0; y<H; y++){
        // convert y -> f
        double f = fmin + ((fmax-fmin)*(double)(H-1-y))/H;
        int fi = floor(f/df);
        if (fi<0) fi=0;
        if (fi>win-2) fi=win-2;
        double v1 = fft.abs(fi);
        double v2 = fft.abs(fi+1);
        double v = (v1 + (f/df-fi)*(v2-v1))/win;
        pic.add(x,y,v/nc);
      }
    }
  }

  Opt opts;
  opts.put("fmin", fmin);
  opts.put("fmax", fmax);
  opts.put("tmin", s.t0);
  opts.put("tmax", s.t0 + s.dt*N);
  opts.put("window", win);
  pic.print_pnm(ff, l, g, opts);
}

/******************************************************************/
void
flt_sfft_pnm_ad(ostream & ff, const Signal & s, const int argc, char **argv) {
  const char *name = "sfft_pnm_ad";

  double fmin=0, fmax=+HUGE_VAL;
  double amin=-HUGE_VAL, amax=+HUGE_VAL;
  int win = 1024;
  int W=1024, H=768, S=16;
  bool l = false;
  const char *g = "KRYW";
  // parse options (opterr==0, optint==1)
  while(1){
    int c = getopt(argc, argv, "+F:G:A:B:W:H:S:lg:");
    if (c==-1) break;
    switch (c){
      case '?': throw Err() << name << ": unknown option: -" << (char)optopt;
      case ':': throw Err() << name << ": no argument: -" << (char)optopt;
      case 'F': fmin = atof(optarg); break;
      case 'G': fmax = atof(optarg); break;
      case 'A': amin = atof(optarg); break;
      case 'B': amax = atof(optarg); break;
      case 'W': W    = atoi(optarg); break;
      case 'H': H    = atoi(optarg); break;
      case 'S': S    = atoi(optarg); break;
      case 'l': l    = true; break;
      case 'g': g    = optarg; break;
    }
  }
  if (argc-optind>0) throw Err() << name << ": extra argument found: " << argv[0];

  int N = s.get_n();
  int cN  = s.get_ch();
  if (N<1 || cN<1) return;
  int ch=0;

  int i1f, i2f;
  double df;

  FFT fft(N);
  fft.get_ind(s.dt, &fmin, &fmax, &i1f, &i2f, &df);

  // min window: we have NM points in fmin:fmax range
  // max window: we have NM points in tmin:tmax range
  int NM = 5;
  int wmin = floor(NM/(fmax-fmin)/s.dt);
  int wmax = ceil(N/NM);

  // first pass
  vector<int> start; // start points for the full calculation
  {
    int win=wmin;
    FFT fft(win);
    fft.get_ind(s.dt, &fmin, &fmax, &i1f, &i2f, &df);
    vector<double> vp(win,0); // previous step data

    // for each x point
    for (int iw=0; iw<N-win; iw+=win){
      fft.run(s.chan[ch].data()+iw, s.chan[ch].sc, true);

      // calculate normalized distance from previous point
      double d0=0, d1=0;
      vector<double> vn(win,0); // new data
      for (int i=i1f; i<i2f; i++){
        vn[i-i1f] = fft.abs(i);
        d1 += pow(vn[i-i1f]-vp[i-i1f], 2);
        d0 += fabs(vn[i-i1f]+vp[i-i1f]);
      }
      bool change = sqrt(d1)/d0>0.05;
      bool longwin = start.size() && (iw+win-start[start.size()-1] > wmax);
      if (change || longwin){
        vp.swap(vn);
        start.push_back(iw);
      }
    }
    // move last point to the data end
    start[start.size()-1] = N;
  }

  // second pass
  dImage pic(W,H,0, amin, amax);
  pic.set_vs(S);
  for (int is = 0; is<start.size()-1; is++){
    int win = start[is+1]-start[is];

    FFT fft(win);
    fft.get_ind(s.dt, &fmin, &fmax, &i1f, &i2f, &df);
    fft.run(s.chan[ch].data()+start[is], s.chan[ch].sc, true);

    size_t x1 = ((double)start[is]*W)/N; // be worry about int overfull
    size_t x2 = ((double)start[is+1]*W)/N;
    for (int y = 0; y<H; y++){
      // convert y -> f
      double f = fmin + ((fmax-fmin)*(H-1-y))/H;
      int fi = floor(f/df);
      if (fi<0) fi=0;
      if (fi>win-2) fi=win-2;
      double v1 = fft.abs(fi);
      double v2 = fft.abs(fi+1);
      double v = (v1 + (f/df-fi)*(v2-v1))/win;
      for (int x=x1; x<x2; x++) pic.set(x,y,v);
    }
  }
  pic.print_pnm(ff, l, g);
}

/******************************************************************/
void
fit(ostream & ff, const Signal & s, const int argc, char **argv) {
  const char *name = "fit";

  double fmin=0, fmax=+HUGE_VAL;
  // parse options (opterr==0, optint==1)
  while(1){
    int c = getopt(argc, argv, "+F:G:");
    if (c==-1) break;
    switch (c){
      case '?': throw Err() << name << ": unknown option: -" << (char)optopt;
      case ':': throw Err() << name << ": no argument: -" << (char)optopt;
      case 'F': fmin = atof(optarg); break;
      case 'G': fmax = atof(optarg); break;
    }
  }
  if (argc-optind>0) throw Err() << name << ": extra argument found: " << argv[0];


  int N  = s.get_n();
  int cN  = s.get_ch();
  if (N<1 || cN<1) return;
  int ch = 0;

  vector<double> ret = ::fit_signal_fixfre(
    s.chan[ch].data(), N, s.chan[ch].sc, s.dt, s.t0, fmin, fmax);

  ff << s.t0abs_str << " "
     << setprecision(12) << ret[0] << " "
     << setprecision(6)  << ret[1] << " "
     << setprecision(6)  << ret[2] << " "
     << setprecision(6)  << ret[3] << "\n";
}

/******************************************************************/
void
fitn(ostream & ff, const Signal & s, const int argc, char **argv) {
  const char *name = "fitn";

  double fmin=0, fmax=+HUGE_VAL;
  int NS = 1; // number of signals to fit
  bool sort_fre = 1;
  // parse options (opterr==0, optint==1)
  while(1){
    int c = getopt(argc, argv, "+F:G:N:s:");
    if (c==-1) break;
    switch (c){
      case '?': throw Err() << name << ": unknown option: -" << (char)optopt;
      case ':': throw Err() << name << ": no argument: -" << (char)optopt;
      case 'F': fmin = atof(optarg); break;
      case 'N': NS = atoi(optarg); break;
      case 'G': fmax = atof(optarg); break;
      case 's': sort_fre = atoi(optarg); break;
    }
  }
  if (argc-optind>0) throw Err() << name << ": extra argument found: " << argv[0];

  int N  = s.get_n();
  int cN  = s.get_ch();
  if (N<1 || cN<1) return;
  int ch = 0;


  std::vector<int> ind; // indices for sorting
  std::vector<double> fre, rel, amp, ph;


  Signal s1 = s; // copy of the signal
  for (int is = 0; is <NS; is++){
    // find signal (largest amplitude)
    vector<double> ret = ::fit_signal_fixfre(
      s1.chan[ch].data(), N, s1.chan[ch].sc, s1.dt, s1.t0, fmin, fmax);

    ind.push_back(is);
    fre.push_back(ret[0]);
    rel.push_back(ret[1]);
    amp.push_back(ret[2]);
    ph.push_back(ret[3]);

    // subtract it
    for (int i=0; i<s1.get_n(); i++){
      double t = s1.t0+i*s1.dt;
      s1.set_val(ch,i, s1.get_val(ch,i) - ret[2]/2.0*exp(-t*ret[1])*sin(2*M_PI*ret[0]*t+ret[3]) );
    }
  }

  // sort results by frequency
  if (sort_fre) {
    class sort_indices{
      private:
        std::vector<double> mparr;
      public:
        sort_indices(std::vector<double> parr) : mparr(parr) {}
        bool operator()(int i, int j) const { return mparr[i]<mparr[j]; }
    };
    std::sort(ind.begin(), ind.end(), sort_indices(fre));
  }

  for (int i=0; i< fre.size(); i++){
    ff << s.t0abs_str << " "
       << setprecision(12) << fre[ind[i]] << " "
       << setprecision(6)  << rel[ind[i]] << " "
       << setprecision(6)  << amp[ind[i]] << " "
       << setprecision(6)  << ph[ind[i]] << "\n";
  }

}

/******************************************************************/
void
sfit(ostream & ff, const Signal & s, const int argc, char **argv) {
  const char *name = "sfit";

  double fmin=0, fmax=+HUGE_VAL;
  int winp = 1024;
  bool win_in_s = false;
  double win;
  double stp = 0;
  int stpp = 0;
  bool midp = false;
  bool use_def_w = true;
  // parse options (opterr==0, optint==1)
  while(1){
    int c = getopt(argc, argv, "+F:G:tw:s:m");
    if (c==-1) break;
    switch (c){
      case '?': throw Err() << name << ": unknown option: -" << (char)optopt;
      case ':': throw Err() << name << ": no argument: -" << (char)optopt;
      case 'F': fmin = atof(optarg); break;
      case 'G': fmax = atof(optarg); break;
      case 't': win_in_s  = true; break;
      case 'w': win = atof(optarg); use_def_w = false; break;
      case 's': stp  = atof(optarg); break;
      case 'm': midp  = true; break;
    }
  }
  if (argc-optind>0) throw Err() << name << ": extra argument found: " << argv[0];
  if (! use_def_w) {
    if (win_in_s) { winp = int(win/s.dt); }
    else { winp = win; }
  };
  if (stp<=0) { stpp = winp; }
  else { stpp=winp*stp/win; }
  if (stpp==0) throw Err() << name << " filter step too small: " << stpp << " points";

  int N  = s.get_n();
  int cN  = s.get_ch();
  if (N<1 || cN<1) return;
  int ch = 0;
  double it0 = winp*0.5*s.dt*int(midp);

  for (int iw=0; iw<N-winp; iw+=stpp){
    vector<double> ret = ::fit_signal_fixfre(
      s.chan[ch].data()+iw, winp, s.chan[ch].sc, s.dt, s.t0, fmin, fmax);

    ff << iw*s.dt + it0 << "\t"
       << setprecision(12) << ret[0] << "\t"
       << setprecision(6)  << ret[1] << "\t"
       << setprecision(6)  << ret[2] << "\t"
       << setprecision(6)  << ret[3] << "\n";
  }

}

/******************************************************************/
void
lockin(ostream & ff, const Signal & s, const int argc, char **argv) {
  const char *name = "lockin";

  double fmin=0, fmax=+HUGE_VAL;
  int ch_sig = 0;
  int ch_ref = 1;
  // parse options (opterr==0, optint==1)
  while(1){
    int c = getopt(argc, argv, "+F:G:s:r:");
    if (c==-1) break;
    switch (c){
      case '?': throw Err() << name << ": unknown option: -" << (char)optopt;
      case ':': throw Err() << name << ": no argument: -" << (char)optopt;
      case 'F': fmin = atof(optarg); break;
      case 'G': fmax = atof(optarg); break;
      case 's': ch_sig = atoi(optarg); break;
      case 'r': ch_ref = atoi(optarg); break;
    }
  }
  if (argc-optind>0) throw Err() << name << ": extra argument found: " << argv[0];

  int N  = s.get_n();
  int cN  = s.get_ch();
  if (N<1) return;
  if (cN<ch_ref || cN<ch_sig) throw Err() << "Signal or reference channel exeeds total number of channels";

  // get frequncy and phase of the reference signal
  vector<double> ret = ::fit_signal_fixfre(
    s.chan[ch_ref].data(), N, s.chan[ch_ref].sc, s.dt, s.t0, fmin, fmax);
  double fre = ret[0];
  double ph  = ret[3];

  double ss1=0, ss2=0;
  for (int i=0; i<N; i++){
    double v = s.chan[ch_sig][i]*s.chan[ch_sig].sc;
    ss1+= v*sin(2*M_PI*fre*s.dt*i + ph);
    ss2+= v*cos(2*M_PI*fre*s.dt*i + ph);
  }
  ff << setprecision(12) << fre << " "
     << setprecision(6)  << 2*ss1/N << " "
     << setprecision(6)  << 2*ss2/N << "\n";

}

/******************************************************************/
void
slockin(ostream & ff, const Signal & s, const int argc, char **argv) {
  const char *name = "slockin";

  double fmin=0, fmax=+HUGE_VAL;
  int win = 1024;
  int ch_sig = 0;
  int ch_ref = 1;
  double fre = 0;
  double ph  = 0;
  // parse options (opterr==0, optint==1)
  while(1){
    int c = getopt(argc, argv, "+F:G:w:r:s:f:p:");
    if (c==-1) break;
    switch (c){
      case '?': throw Err() << name << ": unknown option: -" << (char)optopt;
      case ':': throw Err() << name << ": no argument: -" << (char)optopt;
      case 'F': fmin = atof(optarg); break;
      case 'G': fmax = atof(optarg); break;
      case 'w': win  = atof(optarg); break;
      case 's': ch_sig = atoi(optarg); break;
      case 'r': ch_ref = atoi(optarg); break;
      case 'f': fre    = atof(optarg); break;
      case 'p': ph     = M_PI/180.0*atof(optarg); break;
    }
  }
  if (argc-optind>0) throw Err() << name << ": extra argument found: " << argv[0];


  int N  = s.get_n();
  int cN  = s.get_ch();
  if (N<1) return;
  if (cN<ch_sig) throw Err() << "Signal channel exeeds total number of channels";
  bool use_ref = (fre==0);


  /// for each window
  for (int iw=0; iw<N-win; iw+=win){

    // if frequency is not set, extract frequency and phase from reference channel:
    if (use_ref) {
      if (cN<ch_ref) throw Err() << "Reference channel exeeds total number of channels";

      vector<double> ret = ::fit_signal_fixfre(
        s.chan[ch_ref].data()+iw, win, s.chan[ch_ref].sc, s.dt, 0, fmin, fmax);
      fre = ret[0];
      ph  = ret[3];
    }

    double ss1=0, ss2=0;
    for (int i=iw; i<iw+win; i++){
      double v = s.chan[ch_sig][i]*s.chan[ch_sig].sc;
      ss1+= v*sin(2*M_PI*fre*s.dt*(i-iw) + ph);
      ss2+= v*cos(2*M_PI*fre*s.dt*(i-iw) + ph);
    }
    ff << s.t0 + s.dt*(iw+win/2) << " "
       << setprecision(6)  << 2*ss1/win << " "
       << setprecision(6)  << 2*ss2/win << "\n";
  }
}

/******************************************************************/

void
peak(ostream & ff, const Signal & s, const int argc, char **argv) {
  const char *name = "peak";

  double fmin=0, fmax=+HUGE_VAL;
  bool avr = false;
  // parse options (opterr==0, optint==1)
  while(1){
    int c = getopt(argc, argv, "+F:G:a");
    if (c==-1) break;
    switch (c){
      case '?': throw Err() << name << ": unknown option: -" << (char)optopt;
      case ':': throw Err() << name << ": no argument: -" << (char)optopt;
      case 'F': fmin = atof(optarg); break;
      case 'G': fmax = atof(optarg); break;
      case 'a': avr  = true; break;
    }
  }
  if (argc-optind>0) throw Err() << name << ": extra argument found: " << argv[0];

  int N = s.get_n();
  int cN  = s.get_ch();
  if (N<1 || cN<1) return;

  FFT fft(N);
  int i1f, i2f;
  double df;
  fft.get_ind(s.dt, &fmin, &fmax, &i1f, &i2f, &df);
  std::vector<double> data(i2f-i1f, 0);
  int nc = avr?s.get_ch():1;
  for (int ch = 0; ch<nc; ch++) {
    fft.run(s.chan[ch].data(), s.chan[ch].sc);
    for (int j = i1f; j<i2f; j++) data[j-i1f] += fft.abs(j)/nc;
  }

  // find maximum
  double vm = data[0];
  int im = 0;
  for (int i = 0; i<i2f-i1f; i++){
    double v = data[i];
    if (v>=vm) {vm=v; im=i;}
  }

  // parabolic fit near maximum
  if (im<1 || im>=i2f-i1f-1) return;
  double x1 = -1;
  double x2 = 0;
  double x3 = 1;
  double y1 = data[im-1];
  double y2 = data[im];
  double y3 = data[im+1];
  double A = (y1+y3)/2 - y2;
  double B = (y3-y1)/2;

  cout << df*(i1f + im - B/2/A) << "\n";
}


/******************************************************************/
void
dc(ostream & ff, const Signal & s, const int argc, char **argv) {
  const char *name = "dc";
  if (argc>1) throw Err() << name << ": extra argument found: " << argv[1];

  int N  = s.get_n();
  int cN  = s.get_ch();
  if (N<1 || cN<1) return;

  for (int c = 0; c<cN; c++){
    double avr = 0;
    for (int i=0; i<N; i++){
      avr += s.chan[c][i]*s.chan[c].sc;
    }
    ff << setprecision(6) << avr/N << "\n";
  }
}


/******************************************************************/
void
minmax(ostream & ff, const Signal & s, const int argc, char **argv) {
  const char *name = "minmax";
  if (argc>1) throw Err() << name << ": extra argument found: " << argv[1];

  int N  = s.get_n();
  int cN  = s.get_ch();
  if (N<1 || cN<1) return;

  for (int c = 0; c<cN; c++){
    double min,max;
    min = max = s.chan[c][0]*s.chan[c].sc;
    for (int i=1; i<N; i++){
      double v = s.chan[c][i]*s.chan[c].sc;
      if (v<min) min = v;
      if (v>max) max = v;
    }
    ff << setprecision(6) << min << " "
       << setprecision(6) << max << "\n";
  }
}

/******************************************************************/
void
overload(ostream & ff, const Signal & s, const int argc, char **argv) {
  const char *name = "overload";
  if (argc>1) throw Err() << name << ": extra argument found: " << argv[1];
  int cN  = s.get_ch();
  for (int c = 0; c<cN; c++) ff << (int)s.chan[c].ov << "\n";
}

/******************************************************************/
void
flt_sigf(ostream & ff, const Signal & s, const int argc, char **argv) {
  const char *name = "sigf";

  double fmin=0, fmax=+HUGE_VAL;
  // parse options (opterr==0, optint==1)
  while(1){
    int c = getopt(argc, argv, "+F:G:");
    if (c==-1) break;
    switch (c){
      case '?': throw Err() << name << ": unknown option: -" << (char)optopt;
      case ':': throw Err() << name << ": no argument: -" << (char)optopt;
      case 'F': fmin = atof(optarg); break;
      case 'G': fmax = atof(optarg); break;
    }
  }
  if (argc-optind>0) throw Err() << name << ": extra argument found: " << argv[0];

  write_sigf(ff, s, fmin, fmax);
}

/******************************************************************/
void
flt_sig(ostream & ff, const Signal & s, const int argc, char **argv) {
  const char *name = "sig";
  if (argc>1) throw Err() << name << ": extra argument found: " << argv[1];

  write_sig(ff, s);
}

/******************************************************************/
void
flt_wav(ostream & ff, const Signal & s, const int argc, char **argv) {
  const char *name = "wav";
  if (argc>1) throw Err() << name << ": extra argument found: " << argv[1];

  write_wav(ff, s);
}

/******************************************************************/
void
flt_txt(ostream & ff, const Signal & s, const int argc, char **argv){
  const char *name = "txt";
  if (argc>1) throw Err() << name << ": extra argument found: " << argv[1];
  ff << setprecision(9) << scientific;
  write_txt(ff, s);
}
