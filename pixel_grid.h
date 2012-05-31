#ifndef __PIXEL_GRID__
#define __PIXEL_GRID__

#include<vector>

template<typename F=double> class Point {
 public:
  Point(F xa, F ya):x(xa),y(ya){}    
  F x;
  F y;
};

template<typename I=int> class Index {
 public:
  Index(I xa, I ya):x(xa),y(ya) {}    
  I x;
  I y;
};

template<typename F = double, typename I = int >
class PixelGrid {
public:

  PixelGrid(const Point<F> &ll, const Point<F> &ur, I nx, I ny):
    nx_(nx),ny_(ny),
    pixels_(nx_*ny_) {
    ll_x_=ll.x;
    ll_y_=ll.y;
    ur_x_=ur.x;
    ur_y_=ur.y;

    dx_=(ur_x_-ll_x_)/nx_;
    dy_=(ur_y_-ll_y_)/ny_;
    
  };


  I nx() const {return nx_;}
  I ny() const {return ny_;}

  Point<F> ll() const {return Point<F>(ll_x_,ll_y_);}
  Point<F> ur() const {return Point<F>(ur_x_,ur_y_);}
  

  F dx() const {return dx_;}
  F dy() const {return dy_;}


  Point<F> center(I ix, I iy) const;
  
  Index<I> in(F x, F y) const {
    I ix=(int) floor( (x-ll_x_)/dx_);
    I iy=(int) floor( (y-ll_y_)/dy_);
    return Index<I>(ix,iy);
  };
  
  
  void add(I ix, I iy, F w) {
    
  }

  void insert(F x, F y, F w) {
    
  }
  
  I index(I ix, I iy) const {return iy*nx_+ix;}
  I index(Index<F> ind) const {return index(ind.x,ind.y);}
  
private:
  F  ll_x_, ll_y_;
  F  ur_x_, ur_y_;
  I  nx_, ny_;
  
  std::vector<F> pixels_;
  F dx_,dy_;
  
  

};

#endif
