#include "align.h"
#include <string>
#include <algorithm>
#include <set>

using std::string;
using std::cout;
using std::endl;
using std::min;
using std::max;
using std::tuple;
using std::get;
using std::tie;
using std::make_tuple;
using std::multiset;

enum Constants {
    SHIFT_RANGE_X = 15,
    SHIFT_RANGE_Y = 15
};
typedef long double (*mfunc_t)(Matrix<double> fst, Matrix<double> sec, int x, int y);

double get_y (double red, double green, double blue)
{
    return 0.2125 * red + 0.7154 * green + 0.0721 * blue;
}

Matrix<double> conv2mat (const Image srcImage) //Assuming, that it's a BW Image
{
    long src_sx = static_cast<long>(srcImage.n_rows), src_sy = static_cast<long>(srcImage.n_cols);
    uint r, g, b;
    Matrix<double> converted(srcImage.n_rows, srcImage.n_cols);

    for (int i = 0; i < src_sx; i++) {
        for (int j = 0; j < src_sy; j++) {
            tie(r, g, b) = srcImage(i, j);
            converted(i, j) = static_cast<double>(r+g+b)/3.0;
        }
    }

    return converted;
}


Image conv2img (Matrix<double> red, Matrix<double> green, Matrix<double>blue,\
                int red_shx, int red_shy, int blue_shx, int blue_shy)
{
    int fin_shx = max (max (red_shx, blue_shx), 0);
    int fin_shy = max (max (red_shy, blue_shy), 0);
    int fin_brx = min (min (red_shx+red.n_rows, blue_shx+blue.n_rows), green.n_rows);
    int fin_bry = min (min (red_shy+red.n_cols, blue_shy+blue.n_cols), green.n_cols);

    int dst_sx = fin_brx-fin_shx, dst_sy = fin_bry-fin_shy;
    Image dstImage(dst_sx, dst_sy);

    red   =   red.submatrix (fin_shx -red_shx, fin_shy -red_shy, dst_sx, dst_sy);
    green = green.submatrix (fin_shx         , fin_shy         , dst_sx, dst_sy);
    blue  =  blue.submatrix (fin_shx-blue_shx, fin_shy-blue_shy, dst_sx, dst_sy);

    for (int i = 0; i < dst_sx; i++)
        for (int j = 0; j < dst_sy; j++)
            dstImage(i, j) = make_tuple (red(i, j), green(i, j), blue(i, j) );

    return dstImage;
}

long double metric_mse (Matrix<double> fst, Matrix<double> sec, int x, int y)
{
    int fst_shx = max( x, 0), fst_shy = max( y, 0);
    int sec_shx = max(-x, 0), sec_shy = max(-y, 0);
    double int_sx = min (fst.n_rows-fst_shx, sec.n_rows-sec_shx);
    double int_sy = min (fst.n_cols-fst_shy, sec.n_cols-sec_shy);
    double total = int_sx * int_sy;

    long double mse_sum = 0;
    long double tmp = 0.0;

    for (int i = 0; i < int_sx; i++) {
        for (int j = 0; j < int_sy; j++) {
            tmp = fst(i+fst_shx, j+fst_shy) - sec(i+sec_shx, j+sec_shy);
            mse_sum += (tmp*tmp)/total;
        }
    }
    return mse_sum;
}

long double metric_cc (Matrix<double> fst, Matrix<double> sec, int x, int y)
{
    int fst_shx = max( x, 0), fst_shy = max( y, 0);
    int sec_shx = max(-x, 0), sec_shy = max(-y, 0);
    int int_sx = 0, int_sy = 0;

    int_sx = min (fst.n_rows-fst_shx, sec.n_rows-sec_shx);
    int_sy = min (fst.n_cols-fst_shy, sec.n_cols-sec_shy);

    long double total_cc = 0.0;
    for (int i = 0; i < int_sx; i++)
        for (int j = 0; j < int_sy; j++)
            total_cc += fst(i+fst_shx, j+fst_shy) * sec(i+sec_shx, j+sec_shy);

    return total_cc;
}

tuple<int, int> intersect (Matrix<double> fst, Matrix<double> sec, mfunc_t metric, bool is_max)
{
    //Little bit of intersecting magic - should work properly
    int shift_x = SHIFT_RANGE_X, shift_y = SHIFT_RANGE_Y;
    long double best_metric = metric (fst, sec, shift_x, shift_y), metric_ret = 0.0;
    if (is_max) {
        for (int x = -SHIFT_RANGE_X; x <= SHIFT_RANGE_X; x++) {
            for (int y = -SHIFT_RANGE_Y; y < SHIFT_RANGE_Y; y++) {
                metric_ret = metric (fst, sec, x, y);
                if (metric_ret > best_metric) {
                    shift_x = x;
                    shift_y = y;
                    best_metric = metric_ret;
                }
            }
        }
    } else {
        for (int x = -SHIFT_RANGE_X; x <= SHIFT_RANGE_X; x++) {
            for (int y = -SHIFT_RANGE_Y; y < SHIFT_RANGE_Y; y++) {
                metric_ret = metric (fst, sec, x, y);
                if (metric_ret < best_metric) {
                    shift_x = x;
                    shift_y = y;
                    best_metric = metric_ret;
                }
            }
        }
    }

    return make_tuple(shift_x, shift_y);
}

void shrink_mat(Matrix<double>& matrix, int shrink_rate)
{
    matrix = matrix.submatrix (shrink_rate, shrink_rate, \
                               matrix.n_rows - 2*shrink_rate,\
                               matrix.n_cols - 2*shrink_rate);
}

void shrink_img(Image& matrix, int shrink_rate)
{
    matrix = matrix.submatrix (shrink_rate, shrink_rate, \
                               matrix.n_rows - 2*shrink_rate,\
                               matrix.n_cols - 2*shrink_rate);
}

Image mirror_it (Image srcImage, uint radius)
{
    int src_sx = srcImage.n_rows, src_sy = srcImage.n_cols;
    int dst_sx = src_sx + 2 * radius, dst_sy = src_sy + 2 * radius;
    Image dstImage = Image(dst_sx, dst_sy);
    int vx = (radius/src_sx % 2) ? -1 : 1;
    if (radius % src_sx)
        vx *= -1;

    int vy = (radius/src_sy % 2) ? -1 : 1;
    if (radius % src_sy)
        vy *= -1;

    int curr_x = (vx < 0) ? radius % src_sx : src_sx - (radius % src_sx);
    int curr_y = (vy < 0) ? radius % src_sy : src_sy - (radius % src_sy);
    int start_y = curr_y;

    // cout << src_sx << " " << src_sy << endl;
    // cout << curr_x << " " << curr_y << endl;
    // cout << vx << " " << vy << endl;


    for (int i = 0; i < dst_sx; i++) {
        for (int j = 0; j < dst_sy; j++) {
            // cout << "(" << i << ", " << j << ")" << "-";
            // cout << "(" << curr_x << ", " << curr_y << ")" << endl;

            dstImage (i, j) = srcImage (curr_x, curr_y);
            curr_y += vy;
            if (!curr_y || curr_y == src_sy-1)
                vy *= -1;
        }
        curr_x += vx;
        curr_y = start_y;
        if (!curr_x || curr_x == src_sx-1)
            vx *= -1;
    }
    return dstImage;
}

Image align(Image srcImage, bool isPostprocessing, std::string postprocessingType, double fraction, bool isMirror,
            bool isInterp, bool isSubpixel, double subScale)
{
    uint src_sx = srcImage.n_rows, src_sy = srcImage.n_cols;
    int red_shx = 0, red_shy = 0;
    int blue_shx = 0, blue_shy = 0;

    Matrix<double> blue  = conv2mat (srcImage.submatrix (         0, 0, src_sx/3, src_sy));
    Matrix<double> green = conv2mat (srcImage.submatrix (  src_sx/3, 0, src_sx/3, src_sy));
    Matrix<double> red   = conv2mat (srcImage.submatrix (2*src_sx/3, 0, src_sx/3, src_sy));

    shrink_mat ( blue, 15);
    shrink_mat (green, 15);
    shrink_mat (  red, 15);

    tie ( red_shx,  red_shy) = intersect (green,  red, metric_mse, 0);
    tie (blue_shx, blue_shy) = intersect (green, blue, metric_mse, 0);

    Image my_res = conv2img(red, green, blue, red_shx, red_shy, blue_shx, blue_shy);

    if (isPostprocessing) {
        if (postprocessingType == "--unsharp") {
            my_res = unsharp (my_res, isMirror);
        } else if (postprocessingType == "--gray-world") {
            my_res = gray_world (my_res);
        }
    }

    return my_res;
}

Image sobel_x(Image src_image, bool isMirror) {
    Matrix<double> kernel = {{-1, 0, 1},
                             {-2, 0, 2},
                             {-1, 0, 1}};
    return custom(src_image, kernel, isMirror);
}

Image sobel_y(Image src_image, bool isMirror) {
    Matrix<double> kernel = {{ 1,  2,  1},
                             { 0,  0,  0},
                             {-1, -2, -1}};
    return custom(src_image, kernel, isMirror);
}

Image unsharp(Image src_image, bool isMirror) {
    Matrix<double> kernel = {{-1.0, -4.0, -1.0},
                             {-1.0, 26.0, -4.0},
                             {-1.0, -4.0, -1.0}};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            kernel(i, j) = kernel(i, j)/6.0;
        }
    }
    return custom(src_image, kernel, isMirror);
}

Image gray_world(Image src_image) {
    int src_sx = src_image.n_rows, src_sy = src_image.n_cols;
    double r, g, b;
    long double red_my = 0.0, green_my = 0.0, blue_my = 0.0;
    for (int i = 0; i < src_sx; i++) {
        for (int j = 0; j < src_sy; j++) {
            tie(r, g, b) = src_image(i, j);
            red_my   += r/(1.0*src_sx*src_sy);
            green_my += g/(1.0*src_sx*src_sy);
            blue_my  += b/(1.0*src_sx*src_sy);
        }
    }
    long double total_level = (red_my+green_my+blue_my)/3.0;
    red_my = total_level/red_my;
    green_my = total_level/green_my;
    blue_my = total_level/blue_my;

    Image dst_image = src_image.deep_copy();

    for (int i = 0; i < src_sx; i++) {
        for (int j = 0; j < src_sy; j++) {
            tie(r, g, b) = src_image(i, j);
            r *= red_my;
            g *= green_my;
            b *= blue_my;
            r = min (r, 255.0);
            g = min (g, 255.0);
            b = min (b, 255.0);

            dst_image(i, j) = make_tuple (static_cast<uint>(r),\
                                          static_cast<uint>(g),\
                                          static_cast<uint>(b));
        }
    }

    return dst_image;
}

Image resize(Image src_image, double scale) {
    return src_image;
}

//================================[Inspired by matrix_example.cpp]

class KernelOp {
  public:
      Matrix<double> in_kernel;
      const int radius;
      KernelOp (const Matrix<double> inner_kernel) :
                in_kernel (inner_kernel.deep_copy()),
                radius ( (inner_kernel.n_rows-1)/2)
      {
      }

      tuple<uint, uint, uint> operator () (const Image& img) const
      {
          int size = 2 * radius + 1;
          double r = 0.0, g = 0.0, b = 0.0;
          double sum_r = 0.0, sum_g = 0.0, sum_b = 0.0;
          for (int i = 0; i < size; ++i) {
              for (int j = 0; j < size; ++j) {
                 tie(r, g, b) = img(i, j);

                 sum_r += in_kernel(i, j) * r;
                 sum_g += in_kernel(i, j) * g;
                 sum_b += in_kernel(i, j) * b;
              }
          }

          sum_r = max (min (sum_r, 255.0), 0.0);
          sum_g = max (min (sum_g, 255.0), 0.0);
          sum_b = max (min (sum_b, 255.0), 0.0);

          return make_tuple(static_cast<uint>(sum_r),
                            static_cast<uint>(sum_g),
                            static_cast<uint>(sum_b));
      }
};

Image custom(Image src_image, Matrix<double> kernel, bool isMirror) {
    KernelOp kernel_op = KernelOp(kernel);

    if (isMirror) {
        Image new_src = mirror_it (src_image, kernel_op.radius);
        Image dst_image = new_src.unary_map(kernel_op);
        shrink_img (dst_image, kernel_op.radius);
        return dst_image;
    } else {
        Image dst_image = src_image.unary_map(kernel_op);
        return dst_image;
    }
}

Image autocontrast(Image src_image, double fraction) {
    return src_image;
}

Image gaussian(Image src_image, double sigma, int radius)  {
    return src_image;
}

Image gaussian_separable(Image src_image, double sigma, int radius) {
    return src_image;
}

class MedianOp {
  public:
      const int radius;

      MedianOp (const int rad) :
                radius (rad)
      {
      }

      tuple<uint, uint, uint> operator () (const Image& img) const
      {
          int size = 2 * radius + 1;
          double r = 0, g = 0, b = 0;
          multiset<double> sort_r, sort_g, sort_b;
          sort_r.clear();
          sort_g.clear();
          sort_b.clear();

          for (int i = 0; i < size; ++i) {
              for (int j = 0; j < size; ++j) {
                 tie(r, g, b) = img(i, j);
                 sort_r.insert(r);
                 sort_g.insert(g);
                 sort_b.insert(b);
              }
          }

          size = size*size/2;
          auto r_it = sort_r.begin();
          auto g_it = sort_g.begin();
          auto b_it = sort_b.begin();

          for (int i = 0; i < size; i++) {
              r_it++;
              g_it++;
              b_it++;
          }

          return make_tuple(*r_it, *g_it, *b_it);
      }
};

Image median(Image src_image, int radius) {
    MedianOp median_op = MedianOp(radius);

    Image new_src = mirror_it (src_image, median_op.radius);
    Image dst_image = new_src.unary_map(median_op);
    shrink_img (dst_image, median_op.radius);
    return dst_image;
}

Image median_linear(Image src_image, int radius) {
    return src_image;
}

Image median_const(Image src_image, int radius) {
    return src_image;
}

Image canny(Image src_image, int threshold1, int threshold2) {
    return src_image;
}
