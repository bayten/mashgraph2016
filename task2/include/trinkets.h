#include <array>
#include <tuple>
#include <cmath>
#include "matrix.h"
#include "classifier.h"
#include "EasyBMP.h"
#include "linear.h"
#include "argvparser.h"

//=================================[HOG globals]
const int ANG_SEGS = 16; // Amount of our segments

//P.S. Amounts of cells per X-Y axis are written in ExtractFeatures

//=================================[LBP globals]
int LBPidx[8] = {0, 0, 0, 1, 2, 2, 2, 1}; //Clockwise rotation
int LBPidy[8] = {0, 1, 2, 2, 2, 1, 0, 0};


typedef Matrix<double> GImage; //Grayscale image
typedef Matrix<std::array<double, ANG_SEGS>> HOGArray;
typedef Matrix<std::array<double, 256>> LBPArray;
typedef Matrix<std::array<double, 3>> CFArray;

GImage grayscale_it(BMP* img)
{
    double img_sx = img->TellWidth();
    double img_sy = img->TellHeight();

    GImage res_img = GImage(img_sx, img_sy);
    RGBApixel px;

    for (int x = 0; x < img_sx; ++x) {
    	for (int y = 0; y < img_sy; ++y) {
    		px = img->GetPixel(x, y);
			res_img(x, y) = 0.299 * px.Red + 0.587 * px.Green + 0.114 * px.Blue;
    	}
    }

    return res_img;
}

//================================[Copied from task1]
void shrink_img(GImage& matrix, int shrink_rate)
{
    matrix = matrix.submatrix (shrink_rate, shrink_rate, \
                               matrix.n_rows - 2*shrink_rate,\
                               matrix.n_cols - 2*shrink_rate);
}

GImage mirror_it(GImage srcImage, uint radius)
{
    int src_sx = srcImage.n_rows, src_sy = srcImage.n_cols;
    int dst_sx = src_sx + 2 * radius, dst_sy = src_sy + 2 * radius;
    GImage dstImage = GImage(dst_sx, dst_sy);
    int vx = (radius/src_sx % 2) ? -1 : 1;
    if (radius % src_sx)
        vx *= -1;

    int vy = (radius/src_sy % 2) ? -1 : 1;
    if (radius % src_sy)
        vy *= -1;

    int curr_x = (vx < 0) ? radius % src_sx : src_sx - (radius % src_sx);
    int curr_y = (vy < 0) ? radius % src_sy : src_sy - (radius % src_sy);
    int start_y = curr_y;

    for (int i = 0; i < dst_sx; i++) {
        for (int j = 0; j < dst_sy; j++) {
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

class KernelOp {
  public:
    Matrix<double> in_kernel;
    const int radius;
    const int vert_radius;
    const int hor_radius;
    KernelOp (const Matrix<double> inner_kernel) :
              in_kernel (inner_kernel.deep_copy()),
              radius ( (inner_kernel.n_rows-1)/2),
              vert_radius (radius),
              hor_radius (radius)
    {
    }

    double operator () (const GImage& img) const
    {
        int size = 2 * radius + 1;
        double sum = 0.0;
        
        for (int x = 0; x < size; ++x) 
            for (int y = 0; y < size; ++y) 
                sum += in_kernel (x, y) * img(x, y);

        return sum;
    }
};

GImage custom(GImage src_image, Matrix<double> kernel) {
    KernelOp kernel_op = KernelOp(kernel);

    GImage new_src = mirror_it (src_image, kernel_op.radius);
    GImage dst_image = new_src.unary_map(kernel_op);
    shrink_img (dst_image, kernel_op.radius);

    return dst_image;
}

std::tuple<Matrix<double>, Matrix<double>> get_sobel(GImage img)
{
	Matrix<double> kernel_x = {{-1, 0, 1},
                               {-2, 0, 2},
                               {-1, 0, 1}};
    
	Matrix<double> kernel_y = {{ 1,  2,  1},
                               { 0,  0,  0},
                               {-1, -2, -1}};
    return std::make_tuple(custom(img, kernel_x), custom(img, kernel_y));
}

std::tuple<Matrix<double>, Matrix<double>> get_gradients(GImage img)
{
	int img_sx = img.n_rows, img_sy = img.n_cols;
	double tmp_x = 0.0, tmp_y = 0.0;
	Matrix<double> sobel_x, sobel_y;
	Matrix<double> g_norm   = Matrix<double>(img_sx, img_sy);
	Matrix<double> g_angles = Matrix<double>(img_sx, img_sy);

	std::tie(sobel_x, sobel_y) = get_sobel(img);
	for (int x = 0; x < img_sx; ++x) {
		for (int y = 0; y < img_sy; ++y) {
			tmp_x = sobel_x(x, y);
			tmp_y = sobel_y(x, y);
			g_norm  (x, y) = sqrt(tmp_x*tmp_x + tmp_y*tmp_y);
			g_angles(x, y) = atan2(tmp_y, tmp_x);
		}
	}

	return std::make_tuple (g_norm, g_angles);
}

HOGArray getHOG(GImage img, int cell_x, int cell_y)
{
	Matrix<double> g_norm, g_angles;
	HOGArray HOG_cells = HOGArray(cell_x, cell_y);
	std::tie (g_norm, g_angles) = get_gradients(img);

    // int img_sx = img.n_rows, img_sy = img.n_cols;
    
    // std::cout << "GRADIENT NORMS:";
    // for (int i = 0; i < img_sx; i++) {
    //     std::cout << "[";
    //     for (int j = 0; j < img_sy; j++) {
    //         std::cout << g_norm(i, j) << " ";
    //     }
    //     std::cout << "]" << std::endl;
    // }

    // std::cout << std::endl;
    // std::cout << "GRADIENT ANGLES:";
    // for (int i = 0; i < img_sx; i++) {
    //     std::cout << "[";
    //     for (int j = 0; j < img_sy; j++) {
    //         std::cout << g_angles(i, j) << " ";
    //     }
    //     std::cout << "]" << std::endl;
    // }

    // std::cout << std::endl;


	int cell_sx = img.n_rows/cell_x, cell_sy = img.n_cols/cell_y;
    double pi_seg = 2.0 * M_PI / ANG_SEGS;
    int ang_idx = 0;

	for (int i = 0; i < cell_x; ++i) {
		for (int j = 0; j < cell_y; ++j) {
            
            for (int k = 0; k < ANG_SEGS; ++k)  // Nullify all cumsums
                HOG_cells(i, j)[k] = 0.0;

			for (int x = 0; x < cell_sx; ++x) {
				for (int y = 0; y < cell_sy; ++y) {
					ang_idx = floor( g_angles(i*cell_sx+x, j*cell_sy+y)/pi_seg + ANG_SEGS/2);
                    if (ang_idx >= ANG_SEGS) {
                        ang_idx = ANG_SEGS-1;
                    } else if (ang_idx < 0) {
                        ang_idx = 0;
                    } 
                    HOG_cells(i, j)[ang_idx] += g_norm(i*cell_sx+x, j*cell_sy+y);
				}
			}
		}
	}

    return HOG_cells;
}

void normHOG_euc(HOGArray& HOG_cells)
{
    int cell_x = HOG_cells.n_rows, cell_y = HOG_cells.n_cols;
    
    for (int i = 0; i < cell_x; ++i) {
        for (int j = 0; j < cell_y; ++j) {
            double total = 0.0;  
            for (int k = 0; k < ANG_SEGS; ++k)
                total += HOG_cells(i, j)[k] * HOG_cells(i, j)[k];

            total = sqrt (total);
            if (fabs(total) > 0.0001) {
                for (int k = 0; k < ANG_SEGS; ++k)
                    HOG_cells(i, j)[k] /= total;
            }
        }
    }
}

//=====================================================================[LBP]
int getLBPnum (GImage img)
{
    int threshold = img(1, 1), LBPnum = 0;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
             img(i, j) = (img(i, j) > threshold) ? 1.0 : 0.0;

    for (int i = 0; i < 8; ++i) {
        LBPnum <<= 1;
        LBPnum += img(LBPidx[i], LBPidy[i]);
    }
    return 0;
}

LBPArray getLBP(GImage img, int cell_x, int cell_y)
{
    int img_sx = img.n_rows, img_sy = img.n_cols;
    int cell_sx = img_sx/cell_x, cell_sy = img_sy/cell_y;
    LBPArray LBP_cells = LBPArray (cell_x, cell_y);
    int LBPnum = -1;

    for (int i = 0; i < cell_x; ++i) {
        for (int j = 0; j < cell_y; ++j) {
            
            for (int k = 0; k < 256; ++k)  // Nullify all cumsums
                LBP_cells(i, j)[k] = 0.0;

            GImage cell_img = img.submatrix(i*cell_sx, j*cell_sy, cell_sx, cell_sy);
            cell_img = mirror_it (cell_img, 1);

            for (int x = 1; x < cell_sx+1; ++x) {
                for (int y = 1; y < cell_sy+1; ++y) {
                    LBPnum = getLBPnum (cell_img.submatrix(x-1, y-1, 3, 3));
                    LBP_cells(i, j)[LBPnum] += 1.0;
                }
            }

        }
    }

    return LBP_cells;
}

void normLBP_euc (LBPArray& LBP_cells)
{
    int cell_x = LBP_cells.n_rows, cell_y = LBP_cells.n_cols;
    
    for (int i = 0; i < cell_x; ++i) {
        for (int j = 0; j < cell_y; ++j) {
            double total = 0.0;  
            for (int k = 0; k < 256; ++k)
                total += LBP_cells(i, j)[k] * LBP_cells(i, j)[k];

            total = sqrt (total);
            if (fabs(total) > 0.0001) {
                for (int k = 0; k < 256; ++k)
                    LBP_cells(i, j)[k] /= total;
            }
        }
    }
}

//=====================================================================[CF]

CFArray getCF (BMP* img, int cell_x, int cell_y)
{
    int img_sx = img->TellWidth(), img_sy = img->TellHeight();
    int cell_sx = img_sx/cell_x, cell_sy = img_sy/cell_y;
    int pixels_per_cell = cell_sx*cell_sy;

    CFArray CF_cells = CFArray(cell_x, cell_y);
    RGBApixel px;

    for (int i = 0; i < cell_x; ++i) {
        for (int j = 0; j < cell_y; ++j) {
            double total_red = 0.0, total_green = 0.0, total_blue = 0.0;

            for (int x = 0; x < cell_sx; ++x) {
                for (int y = 0; y < cell_sy; ++y) {
                    px = img->GetPixel(i*cell_sx+x, j*cell_sy+y);
                    total_red   += px.Red;
                    total_green += px.Green;
                    total_blue  += px.Blue;
                }
            }
            CF_cells(i, j)[0] = total_red  /pixels_per_cell/255.0;
            CF_cells(i, j)[1] = total_green/pixels_per_cell/255.0;
            CF_cells(i, j)[2] = total_blue /pixels_per_cell/255.0;

        }
    }
    return CF_cells;
}