Status X86_VID2COL(float* src, int channel, int depth,int height, int width,int kernelh, int kernelw, int kerneld, int padl, int padr,
                  int padt, int padb,int padf,int padback ,int strideh, int stridew, int strided,int dilationh, int dilationw, int dilationd,float* dst) {
    
    //height、width、depth: 输入特征图的高、宽、深度
    //输出特征图的尺寸
    int height_col   = (height + padt + padb - dilationh * (kernelh - 1) - 1) / strideh + 1;
    int width_col    = (width +  padl + padr - dilationw * (kernelw - 1) - 1) / stridew + 1;
    int depth_col    = (depth +  padf + padback - dilationd * (kerneld -1) - 1) / strided + 1;

    //计算矩阵一列的长度 (c_in*k_w*k_h*k_d) 矩阵的行数
    int channels_col = channel * kernelh * kernelw * kerneld;
    // im2col
    for (int c = 0; c < channels_col; c++) {
        int w_offset = c % kernelw;                       //第几行
        int h_offset = (c / kernelw) % kernelh;           //第几列
        int d_offset = (c / kernelw / kernelh) % kerneld; //第几帧
        int c_im     = c / kernelh / kernelw / kerneld;   //第几个输入特征图

        int h_base = h_offset * dilationh - padt; //选取核大小区域的行初始位置（如果有pad，则为负，没有在原始数组中补充0）
        int w_base = w_offset * dilationw - padl; //选取核大小区域的列初始位置（如果有pad，则为负，没有在原始数组中补充0）
        int d_base = d_offset * dilationd - padf; //选取核大小区域的d初始位置

        int h_base_start = MAX(0, (UP_DIV(-h_base, strideh)));
        int h_base_end   = MIN(height_col, UP_DIV(height - h_base, strideh));
        int w_base_start = MAX(0, (UP_DIV(-w_base, stridew)));
        int w_base_end   = MIN(width_col, UP_DIV(width - w_base, stridew));
        int d_base_start = MAX(0,(UP_DIV(-d_base,strided)));
        int d_base_end   = MIN(depth_col, UP_DIV(depth - d_base, strided));

        auto src_c = src + c_im * height * width * depth;
        auto dst_c = dst + c * height_col * width_col * depth_col;

        // memset(dst_c, 0, h_base_start * width_col * depth_col *sizeof(float));
        // memset(dst_c, 0, h_base_start * width_col *sizeof(float));
        for(int d = d_base_start;d<d_base_end;d++)
        {   
            //d_base =  d_offset * dilationd - padf
            int d_pad = d_base + d*strided;

            //遍历一个通道特征图
            for (int h = h_base_start; h < h_base_end; h++) {
                int h_pad = h_base + h * strideh;


                auto src_h = src_c + (d_pad*height + h_pad)*width;
                auto dst_h = dst_c + (d*height_col + h)*width_col ;

                for (int w = 0; w < w_base_start; w++) {
                    dst_h[w] = 0;
                }
                for (int w = w_base_start; w < w_base_end; w++) {
                    int w_pad = w_base + w * stridew;
                    dst_h[w]  = src_h[w_pad];
                }
                for (int w = w_base_end; w < width_col; w++) {
                    dst_h[w] = 0;
                }
            }
        }
        //memset(dst_c + h_base_end * width_col, 0, (height_col - h_base_end) * width_col * sizeof(float));
        //memset(dst_c + (d_base_end *height_col + h_base_end)*width_col, 0, ((depth_col - d_base_end)*height_col + (height_col - h_base_end)) * width_col * sizeof(float));

    }                                                                      
    return TNN_OK;
}
