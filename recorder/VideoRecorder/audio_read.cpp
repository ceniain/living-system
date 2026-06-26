#include "audio_read.h"
Audio_Read::Audio_Read()
{
    //声卡采样格式
    // set up the format you want, eg.
    format.setSampleRate(AudioCollectFrequency);
    format.setChannelCount(2);
    format.setSampleSize(16);
    format.setCodec("audio/pcm");
    format.setByteOrder(QAudioFormat::LittleEndian);
    //format.setByteOrder(QAudioFormat::BigEndian);
    format.setSampleType(QAudioFormat::SignedInt);///**************
    //format.setSampleType(QAudioFormat::SignedInt);
    QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
    if (!info.isFormatSupported(format)) {
        format = info.nearestFormat(format);
        qDebug() << "原始格式不支持，已自动调整为最近格式：" << format;
    }
    if (!info.isFormatSupported(format)) {
        QMessageBox::critical(NULL , "错误", "音频设备不支持任何可用格式，无法录音");
    }
    audio_in = NULL;
    m_playState = state_stop;
    timer = new QTimer;
    connect( timer , SIGNAL(timeout())
                       , this , SLOT(slot_readMore()) );
    m_buffPos = 0;
}
Audio_Read::~Audio_Read()
{
    this->UnInit();
    delete audio_in;
}
//关于数据点个数 aac 采样点 一帧 1024 频率 44.1kHz 那么采样间隔就是 1024/44.1= 23.2 ms
//一次读取两帧 aac 即 2048 个采样点
//void Audio_Read::slot_readMore()
//{
//    // ///视频没有开始采集 不采集音频
//    if (!audio_in)
//        return;
//    std::vector<uint8_t> multipacketbuffer;
//    qint64 len = audio_in->bytesReady();
//    if (len < OneAudioSize )
//    {
//        return;
//    }
//    //一次性都读出来
//    if( m_buffPos == 0)
//    {
//        m_audiobuff.resize( len );
//    }else
//    {
//        int nSize = m_audiobuff.size();
//        uint8_t* pBegin = &*m_audiobuff.begin();
//        std::vector<uint8_t> tempbuff(nSize - m_buffPos , 0 );
//        uint8_t* pTemp = &*tempbuff.begin();
//        //先保存到临时空间 , 然后申请新空间
//        memcpy( pTemp , pBegin + m_buffPos , nSize - m_buffPos);
//        m_audiobuff.resize( len + tempbuff.size() );
//        pBegin = &*m_audiobuff.begin();
//        memcpy( pBegin , pTemp , tempbuff.size() );
//        m_buffPos = tempbuff.size();
//    }
//    uint8_t* pAudioBuff = &*m_audiobuff.begin() + m_buffPos;
//    int offset = 0;
//    int packSize = len;
//    //数组首地址
//    // uint8_t * pInData = (uint8_t*)&*m_buffer.begin() ;
//    //这次不读走 后面就丢了
//    while( packSize ){
//        qint64 l = myBuffer_in->read( (char*)(pAudioBuff + offset) , packSize );
//        if( l > 0 )
//        {
//            packSize -= l;
//            offset += l;
//        }
//    }
//    if (m_audiobuff.size() > 16)
//    {
//        qDebug() << "[RAW] m_audiobuff前16字节:"
//                 << (int)m_audiobuff[0] << (int)m_audiobuff[1]
//                 << (int)m_audiobuff[2] << (int)m_audiobuff[3]
//                 << (int)m_audiobuff[4] << (int)m_audiobuff[5]
//                 << (int)m_audiobuff[6] << (int)m_audiobuff[7];
//    }
//    //积累了多少帧
//    int nFrameCount = ( m_audiobuff.size()/OneAudioSize );
//    // qDebug() << "nFrameCount = " << nFrameCount;
//    multipacketbuffer.resize( OneAudioSize*nFrameCount );
//    //所有帧数组首地址
//    uint8_t * in_buffer = (uint8_t*)&*multipacketbuffer.begin() ;
//    memcpy( in_buffer , &*m_audiobuff.begin() , OneAudioSize*nFrameCount );
//    m_buffPos = OneAudioSize*nFrameCount;
//    if( m_buffPos == m_audiobuff.size() )
//        m_buffPos = 0;
//    /// nb_samples 和 frame_size = 1024
//    /// nb_samples 采样数
//    ///一帧数据量：1024*2*av_get_bytes_per_sample(s16) = 4096 个字节。
//    ///会编码：88200/(1024*2*av_get_bytes_per_sample(s16)) = 21.5 帧数据
//    /// aac 规定每帧音频只能有 1024 个采样
//    //采集的 pcm 数据是 s16 模式 是 uint 而编码的时候, 使用的是 fltp 需要转换 使用
//    //swr_convert
//        //1024 字节 元素 float *4
//        uint8_t * out_buffer[2]; //左右声道数据
//    out_buffer[0] = (uint8_t *)malloc( OneAudioSize*nFrameCount );
//    memset( out_buffer[0] , 0 ,OneAudioSize*nFrameCount );
//    out_buffer[1] = (uint8_t *)malloc( OneAudioSize*nFrameCount );
//    memset( out_buffer[1] , 0 ,OneAudioSize*nFrameCount );
//    // 关于长度 采样数 即 1024
//    int count =swr_convert(swr, out_buffer , nFrameCount*OneAudioSize/4,
//                            (const uint8_t
//                                 **)&/*pInData*/in_buffer,nFrameCount*OneAudioSize/4);//len 为 4096
//    //每次 读取 一帧
//    for( int n = 0 ; n < nFrameCount ; n++)
//    {
//        uint8_t * audio_left = out_buffer[0] + n* OneAudioSize ;
//        uint8_t * audio_right = out_buffer[1] + n* OneAudioSize ;
//        //pInData 存储 PCM 数据 长度 4096 小端
//        //转换为 ffmpeg FLTP 平面模式 LLLLLLLLLLLLLLLLRRRRRRRRRRRRRRRR
//        uint8_t * buffer = (uint8_t *)malloc( OneAudioSize * AudioChannelCount );
//        memset( buffer , 0 ,OneAudioSize * AudioChannelCount );
//        //第一帧左声道
//        memcpy( buffer , audio_left , OneAudioSize );
//        //第一帧右声道
//        memcpy( buffer + OneAudioSize , audio_right , OneAudioSize );
//        Q_EMIT SIG_sendAudioFrameData( (uint8_t*)buffer, OneAudioSize *
//                                                              AudioChannelCount );
//    }
//    delete [] out_buffer[0];
//    delete [] out_buffer[1];
//}
void Audio_Read::slot_readMore()
{
    if (!audio_in) {
        qDebug() << "[AUDIO_DEBUG] 错误: audio_in 为空指针";
        return;
    }

    qint64 len = audio_in->bytesReady();
    if (len < OneAudioSize) {
        return; // 数据不够，静默返回
    }

//    qDebug() << "==================== [AUDIO FRAME START] ====================";
//    qDebug() << "[AUDIO_INPUT] 硬件缓冲区可读字节 len =" << len << "| 历史残留 m_buffPos =" << m_buffPos;

    // 1. 读取原始数据并拼接到缓冲区尾部
    int old_size = (m_buffPos == 0) ? 0 : m_buffPos;
    m_audiobuff.resize(old_size + len);

    uint8_t* pAudioBuff = m_audiobuff.data() + old_size;
    int offset = 0;
    int packSize = len;
    while (packSize > 0) {
        qint64 l = myBuffer_in->read((char*)(pAudioBuff + offset), packSize);
        if (l > 0) {
            packSize -= l;
            offset += l;
        } else {
            qDebug() << "[AUDIO_INPUT] 警告: 底层 read 返回了非正数:" << l;
            break;
        }
    }

    // 窥探刚读取完的输入端原始 PCM S16 信号波形
    if (m_audiobuff.size() >= 8) {
        int16_t* pRawS16 = (int16_t*)m_audiobuff.data();
        //qDebug() << "[AUDIO_RAW_PCM] 原始前4个采样点 (S16) 振幅:"
                 //<< pRawS16[0] << pRawS16[1] << pRawS16[2] << pRawS16[3];
    }

    // 2. 计算并切出整帧
    int in_bytes_per_sample = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16); // 2
    int in_channels = 2;
    int samples_per_frame = 1024;
    int one_frame_bytes = samples_per_frame * in_channels * in_bytes_per_sample; // 4096

    int nFrameCount = m_audiobuff.size() / one_frame_bytes;
//    qDebug() << "[AUDIO_PROCESS] 混合缓冲区总大小 =" << m_audiobuff.size()
//             << "字节 | 可切出标准 FFmpeg 音频帧数 nFrameCount =" << nFrameCount;

    if (nFrameCount == 0) {
//        qDebug() << "[AUDIO_PROCESS] 字节不足一帧 (" << one_frame_bytes << ")，暂不处理，等待继续攒数据";
//        qDebug() << "==================== [AUDIO FRAME END] ====================\n";
        return;
    }

    int consumed_bytes = one_frame_bytes * nFrameCount;
    std::vector<uint8_t> multipacketbuffer(consumed_bytes);
    memcpy(multipacketbuffer.data(), m_audiobuff.data(), consumed_bytes);

    // 【核心修正】移走已消耗的字节，更新残留大小
    int residual_bytes = m_audiobuff.size() - consumed_bytes;
    if (residual_bytes > 0) {
        memmove(m_audiobuff.data(), m_audiobuff.data() + consumed_bytes, residual_bytes);
        m_buffPos = residual_bytes;
    } else {
        m_buffPos = 0;
    }
//    qDebug() << "[AUDIO_MEMORY] 成功消耗 =" << consumed_bytes << "字节 | 滚动裁剪后留给下次的残留 m_buffPos =" << m_buffPos;

    // 3. 分配重采样空间
    int out_bytes_per_sample = av_get_bytes_per_sample(AV_SAMPLE_FMT_FLTP); // 4
    int total_out_samples_per_channel = nFrameCount * samples_per_frame;
    int out_buffer_size = total_out_samples_per_channel * out_bytes_per_sample;

    uint8_t *out_buffer[2];
    out_buffer[0] = (uint8_t *)malloc(out_buffer_size);
    out_buffer[1] = (uint8_t *)malloc(out_buffer_size);

    // 4. 重采样
    int converted = swr_convert(swr, out_buffer, total_out_samples_per_channel,
                                (const uint8_t**)&multipacketbuffer, total_out_samples_per_channel);

//    qDebug() << "[AUDIO_RESAMPLE] Swr 转换完成。预期单声道采样点 =" << total_out_samples_per_channel
//             << "| 实际转换成功单声道采样点 =" << converted;

    if (converted < 0) {
        qDebug() << "[AUDIO_RESAMPLE] !!! 错误: swr_convert 重采样执行失败";
        free(out_buffer[0]);
        free(out_buffer[1]);
        //qDebug() << "==================== [AUDIO FRAME END] ====================\n";
        return;
    }

    // 5. 分帧组装发送
    if (converted >= 0) {
        int out_one_channel_frame_bytes = samples_per_frame * out_bytes_per_sample; // 4096
        int emitCount = 0;

        for (int n = 0; n < nFrameCount; n++) {
            uint8_t *audio_left = out_buffer[0] + n * out_one_channel_frame_bytes;
            uint8_t *audio_right = out_buffer[1] + n * out_one_channel_frame_bytes;

            int send_size = out_one_channel_frame_bytes * in_channels; // 8192
            uint8_t *buffer = (uint8_t *)malloc(send_size);

            memcpy(buffer, audio_left, out_one_channel_frame_bytes);
            memcpy(buffer + out_one_channel_frame_bytes, audio_right, out_one_channel_frame_bytes);

            // 窥探第一帧重采样转换成 FLTP 后的浮点信号（通常在 -1.0 到 1.0 之间）
            if (n == 0) {
                float* pFloatL = (float*)audio_left;
                float* pFloatR = (float*)audio_right;
//                qDebug() << "[AUDIO_OUTPUT_FLTP] 首帧前2个采样点左声道浮点振幅:" << pFloatL[0] << pFloatL[1];
//                qDebug() << "[AUDIO_OUTPUT_FLTP] 首帧前2个采样点右声道浮点振幅:" << pFloatR[0] << pFloatR[1];
            }

            Q_EMIT SIG_sendAudioFrameData(buffer, send_size);
            emitCount++;
        }
//        qDebug() << "[AUDIO_DISPATCH] 信号分发完毕，本次成功抛出 SIG_sendAudioFrameData 信号次数 =" << emitCount;
    }

    free(out_buffer[0]);
    free(out_buffer[1]);
//    qDebug() << "==================== [AUDIO FRAME END] ====================\n";
}

void Audio_Read::UnInit()
{
    if(timer)
        timer->stop();
    if(audio_in)
        audio_in->stop();
}
void Audio_Read:: slot_openAudio()
{
//    qDebug() << "实际使用的音频格式："
//             << "采样率：" << format.sampleRate()
//             << "声道数：" << format.channelCount()
//             << "位深：" << format.sampleSize()
//             << "类型：" << (format.sampleType() == QAudioFormat::SignedInt ? "SignedInt" : "UnSignedInt");
    if( m_playState == state_stop )
    {
        audio_in = new QAudioInput(format, this);
        myBuffer_in = audio_in->start();
    }
    else
    {
        if(audio_in)
        {
            delete audio_in;
            audio_in = new QAudioInput(format, this);
            myBuffer_in = audio_in->start();
        }
    }
    m_playState = state_play;
    //重采样设置选项----------------
    enum AVSampleFormat in_sample_fmt; //输入的采样格式
    enum AVSampleFormat out_sample_fmt;//输出的采样格式 16bit PCM
    int in_sample_rate;//输入的采样率
    int out_sample_rate;//输出的采样率
    int in_ch_layout;//输入的声道布局
    int out_ch_layout;//输入的声道布局
    //输入的采样格式
    in_sample_fmt = AV_SAMPLE_FMT_S16;
    //输出的采样格式 32bit PCM
    out_sample_fmt = AV_SAMPLE_FMT_FLTP;
    //输入的采样率
    in_sample_rate = AudioCollectFrequency;
    //输出的采样率
    out_sample_rate = 44100/*48000*/;
    //输入的声道布局
    in_ch_layout = AV_CH_LAYOUT_STEREO;
    //输出的声道布局
    out_ch_layout = AV_CH_LAYOUT_STEREO;
    swr = swr_alloc_set_opts(nullptr, out_ch_layout, out_sample_fmt, out_sample_rate,
                             in_ch_layout, in_sample_fmt, in_sample_rate,
                             0, nullptr);
    swr_init(swr);
    timer->start( AUDIO_INTERVAL );
}
void Audio_Read:: slot_closeAudio()
{
    if(timer){
        timer->stop();
    }
    if( m_playState == state_play )
    {
        m_playState = state_pause;
        if(audio_in)
            audio_in->stop();
    }
}
//#include "audio_read.h"
//Audio_Read::Audio_Read()
//{
//    //声卡采样格式
//    // set up the format you want, eg.
//    format.setSampleRate(AudioCollectFrequency);
//    format.setChannelCount(2);
//    format.setSampleSize(16);
//    format.setCodec("audio/pcm");
//    format.setByteOrder(QAudioFormat::LittleEndian);
//    //format.setByteOrder(QAudioFormat::BigEndian);
//    format.setSampleType(QAudioFormat::UnSignedInt);
//    //format.setSampleType(QAudioFormat::SignedInt);
//    QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
//    if (!info.isFormatSupported(format)) {
//        // qWarning()<<"default format not supported try to use nearest";
//        QMessageBox::information(NULL , "提示", "打开音频设备失败");
//        format = info.nearestFormat(format);
//    }
//    audio_in = NULL;
//    m_playState = state_stop;
//    timer = new QTimer;
//    connect( timer , SIGNAL(timeout())
//                       , this , SLOT(slot_readMore()) );
//    m_buffPos = 0;
//}
//Audio_Read::~Audio_Read()
//{
//    this->UnInit();
//    delete audio_in;
//}
////关于数据点个数 aac 采样点 一帧 1024 频率 44.1kHz 那么采样间隔就是 1024/44.1= 23.2 ms
////一次读取两帧 aac 即 2048 个采样点
//void Audio_Read::slot_readMore()
//{
//    // ///视频没有开始采集 不采集音频
//    if (!audio_in)
//        return;
//    std::vector<uint8_t> multipacketbuffer;
//    qint64 len = audio_in->bytesReady();
//    if (len < OneAudioSize )
//    {
//        return;
//    }
//    //一次性都读出来
//    if( m_buffPos == 0)
//    {
//        m_audiobuff.resize( len );
//    }else
//    {
//        int nSize = m_audiobuff.size();
//        uint8_t* pBegin = &*m_audiobuff.begin();
//        std::vector<uint8_t> tempbuff(nSize - m_buffPos , 0 );
//        uint8_t* pTemp = &*tempbuff.begin();
//        //先保存到临时空间 , 然后申请新空间
//        memcpy( pTemp , pBegin + m_buffPos , nSize - m_buffPos);
//        m_audiobuff.resize( len + tempbuff.size() );
//        pBegin = &*m_audiobuff.begin();
//        memcpy( pBegin , pTemp , tempbuff.size() );
//        m_buffPos = tempbuff.size();
//    }
//    uint8_t* pAudioBuff = &*m_audiobuff.begin() + m_buffPos;
//    int offset = 0;
//    int packSize = len;
//    //数组首地址
//    // uint8_t * pInData = (uint8_t*)&*m_buffer.begin() ;
//    //这次不读走 后面就丢了
//    while( packSize ){
//        qint64 l = myBuffer_in->read( (char*)(pAudioBuff + offset) , packSize );
//        if( l > 0 )
//        {
//            packSize -= l;
//            offset += l;
//        }
//    }
//    //积累了多少帧
//    int nFrameCount = ( m_audiobuff.size()/OneAudioSize );
//    // qDebug() << "nFrameCount = " << nFrameCount;
//    multipacketbuffer.resize( OneAudioSize*nFrameCount );
//    //所有帧数组首地址
//    uint8_t * in_buffer = (uint8_t*)&*multipacketbuffer.begin() ;
//    memcpy( in_buffer , &*m_audiobuff.begin() , OneAudioSize*nFrameCount );
//    m_buffPos = OneAudioSize*nFrameCount;
//    if( m_buffPos == m_audiobuff.size() )
//        m_buffPos = 0;
//    /// nb_samples 和 frame_size = 1024
//    /// nb_samples 采样数
//    ///一帧数据量：1024*2*av_get_bytes_per_sample(s16) = 4096 个字节。
//    ///会编码：88200/(1024*2*av_get_bytes_per_sample(s16)) = 21.5 帧数据
//    /// aac 规定每帧音频只能有 1024 个采样
//    //采集的 pcm 数据是 s16 模式 是 uint 而编码的时候, 使用的是 fltp 需要转换 使用
//    //swr_convert
//    //1024 字节 元素 float *4
//    uint8_t * out_buffer[2]; //左右声道数据
//    out_buffer[0] = (uint8_t *)malloc( OneAudioSize*nFrameCount );
//    memset( out_buffer[0] , 0 ,OneAudioSize*nFrameCount );
//    out_buffer[1] = (uint8_t *)malloc( OneAudioSize*nFrameCount );
//    memset( out_buffer[1] , 0 ,OneAudioSize*nFrameCount );
//    // 关于长度 采样数 即 1024
//    int count =swr_convert(swr, out_buffer , nFrameCount*OneAudioSize/4,
//                            (const uint8_t
//                                 **)&/*pInData*/in_buffer,nFrameCount*OneAudioSize/4);//len 为 4096
//    //每次 读取 一帧
//    for( int n = 0 ; n < nFrameCount ; n++)
//    {
//        uint8_t * audio_left = out_buffer[0] + n* OneAudioSize ;
//        uint8_t * audio_right = out_buffer[1] + n* OneAudioSize ;
//        //pInData 存储 PCM 数据 长度 4096 小端
//        //转换为 ffmpeg FLTP 平面模式 LLLLLLLLLLLLLLLLRRRRRRRRRRRRRRRR
//        uint8_t * buffer = (uint8_t *)malloc( OneAudioSize * AudioChannelCount );
//        memset( buffer , 0 ,OneAudioSize * AudioChannelCount );
//        //第一帧左声道
//        memcpy( buffer , audio_left , OneAudioSize );
//        //第一帧右声道
//        memcpy( buffer + OneAudioSize , audio_right , OneAudioSize );
//        Q_EMIT SIG_sendAudioFrameData( (uint8_t*)buffer, OneAudioSize *
//                                                              AudioChannelCount );
//    }
//    delete [] out_buffer[0];
//    delete [] out_buffer[1];
//}
//void Audio_Read::UnInit()
//{
//    if(timer)
//        timer->stop();
//    if(audio_in)
//        audio_in->stop();
//}
//void Audio_Read:: slot_openAudio()
//{
//    if( m_playState == state_stop )
//    {
//        audio_in = new QAudioInput(format, this);
//        myBuffer_in = audio_in->start();
//    }
//    else
//    {
//        if(audio_in)
//        {
//            delete audio_in;
//            audio_in = new QAudioInput(format, this);
//            myBuffer_in = audio_in->start();
//        }
//    }
//    m_playState = state_play;
//    //重采样设置选项----------------
//    enum AVSampleFormat in_sample_fmt; //输入的采样格式
//    enum AVSampleFormat out_sample_fmt;//输出的采样格式 16bit PCM
//    int in_sample_rate;//输入的采样率
//    int out_sample_rate;//输出的采样率
//    int in_ch_layout;//输入的声道布局
//    int out_ch_layout;//输入的声道布局
//    //输入的采样格式
//    in_sample_fmt = AV_SAMPLE_FMT_S16;
//    //输出的采样格式 32bit PCM
//    out_sample_fmt = AV_SAMPLE_FMT_FLTP;
//    //输入的采样率
//    in_sample_rate = AudioCollectFrequency;
//    //输出的采样率
//    out_sample_rate = 44100/*48000*/;
//    //输入的声道布局
//    in_ch_layout = AV_CH_LAYOUT_STEREO;
//    //输出的声道布局
//    out_ch_layout = AV_CH_LAYOUT_STEREO;
//    swr = swr_alloc_set_opts(nullptr, out_ch_layout, out_sample_fmt, out_sample_rate,
//                             in_ch_layout, in_sample_fmt, in_sample_rate,
//                             0, nullptr);
//    swr_init(swr);
//    timer->start( AUDIO_INTERVAL );
//}
//void Audio_Read:: slot_closeAudio()
//{
//    if(timer){
//        timer->stop();
//    }
//    if( m_playState == state_play )
//    {
//        m_playState = state_pause;
//        if(audio_in)
//            audio_in->stop();
//    }
//}
