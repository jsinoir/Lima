#include "HwBufferMgr.h"
#include "MemUtils.h"
#include <cstring>

using namespace lima;

/*******************************************************************
 * \brief BufferAllocMgr destructor
 *******************************************************************/

BufferAllocMgr::~BufferAllocMgr()
{
}

void BufferAllocMgr::clearBuffer(int buffer_nb)
{
	void *ptr = getBufferPtr(buffer_nb);
	const FrameDim& frame_dim = getFrameDim();
	memset(ptr, 0, frame_dim.getMemSize());
}

void BufferAllocMgr::clearAllBuffers()
{
	for (int i = 0; i < getNbBuffers(); i++)
		clearBuffer(i);
}


/*******************************************************************
 * \brief SoftBufferAllocMgr constructor
 *******************************************************************/

SoftBufferAllocMgr::SoftBufferAllocMgr()
{
}

SoftBufferAllocMgr::~SoftBufferAllocMgr()
{
	releaseBuffers();
}

int SoftBufferAllocMgr::getMaxNbBuffers(const FrameDim& frame_dim)
{
	int frame_size = frame_dim.getMemSize();
	if (frame_size <= 0)
		throw LIMA_HW_EXC(InvalidValue, "Invalid FrameDim");

	int tot_buffers;
	GetSystemMem(frame_size, tot_buffers);
	return (long long) tot_buffers * 3 / 4;
}

void SoftBufferAllocMgr::allocBuffers(int nb_buffers, 
				 const FrameDim& frame_dim)
{
	int frame_size = frame_dim.getMemSize();
	if (frame_size <= 0)
		throw LIMA_HW_EXC(InvalidValue, "Invalid FrameDim");

	if ((frame_dim == m_frame_dim) && (nb_buffers == getNbBuffers()))
		return;

	releaseBuffers();

	int max_buffers = getMaxNbBuffers(frame_dim);
	if ((nb_buffers < 1) || (nb_buffers > max_buffers))
		throw LIMA_HW_EXC(InvalidValue, "Invalid number of buffers");

	try {
		m_buffer_list.reserve(nb_buffers);
		for (int i = 0; i < nb_buffers; ++i) 
			m_buffer_list.push_back(new char[frame_size]);
	} catch (...) {
		releaseBuffers();
		throw;
	}

	m_frame_dim = frame_dim;
}

void SoftBufferAllocMgr::releaseBuffers()
{
	BufferList& bl = m_buffer_list;
	for (BufferListCIt it = bl.begin(); it != bl.end(); ++it)
		delete [] *it;
	bl.clear();
	m_frame_dim = FrameDim();
}

const FrameDim& SoftBufferAllocMgr::getFrameDim()
{
	return m_frame_dim;
}

int SoftBufferAllocMgr::getNbBuffers()
{
	return m_buffer_list.size();
}

void *SoftBufferAllocMgr::getBufferPtr(int buffer_nb)
{
	return m_buffer_list[buffer_nb];
}


/*******************************************************************
 * \brief BufferCbMgr destructor
 *******************************************************************/

BufferCbMgr::~BufferCbMgr()
{
}

/*******************************************************************
 * \brief StdBufferCbMgr constructor
 *******************************************************************/

StdBufferCbMgr::StdBufferCbMgr(BufferAllocMgr& alloc_mgr)
	: m_alloc_mgr(alloc_mgr)

{
	m_fcb_act = false;
}

StdBufferCbMgr::~StdBufferCbMgr()
{
}

BufferCbMgr::Cap StdBufferCbMgr::getCap()
{
	return Basic;
}

int StdBufferCbMgr::getMaxNbBuffers(const FrameDim& frame_dim)
{
	return m_alloc_mgr.getMaxNbBuffers(frame_dim);
}

void StdBufferCbMgr::allocBuffers(int nb_buffers, 
				  const FrameDim& frame_dim)
{
	int frame_size = frame_dim.getMemSize();
	if (frame_size <= 0)
		throw LIMA_HW_EXC(InvalidValue, "Invalid FrameDim");

	if ((frame_dim == getFrameDim()) && (nb_buffers == getNbBuffers()))
		return;

	releaseBuffers();

	try {
		m_alloc_mgr.allocBuffers(nb_buffers, frame_dim);

		m_info_list.reserve(nb_buffers);
		for (int i = 0; i < nb_buffers; ++i)
			m_info_list.push_back(HwFrameInfoType());
	} catch (...) {
		releaseBuffers();
		throw;
	}
}

void StdBufferCbMgr::releaseBuffers()
{
	m_alloc_mgr.releaseBuffers();
	m_info_list.clear();
}

void StdBufferCbMgr::setStartTimestamp(Timestamp start_ts)
{
	if (!start_ts.isSet())
		throw LIMA_HW_EXC(InvalidValue, "Invalid start timestamp");
	m_start_ts = start_ts;
}

void StdBufferCbMgr::getStartTimestamp(Timestamp& start_ts) 
{
	start_ts = m_start_ts;
}

void StdBufferCbMgr::setFrameCallbackActive(bool cb_active)
{
	m_fcb_act = cb_active;
}

bool StdBufferCbMgr::newFrameReady(HwFrameInfoType& frame_info)
{
	Timestamp now = Timestamp::now();
	if (!frame_info.frame_timestamp.isSet())
		frame_info.frame_timestamp = now - m_start_ts;

        int buffer_nb = frame_info.acq_frame_nb % getNbBuffers();
	void *ptr = getBufferPtr(buffer_nb);
	if (!frame_info.frame_ptr)
		frame_info.frame_ptr = ptr;
	else if (frame_info.frame_ptr != ptr)
		throw LIMA_HW_EXC(InvalidValue, "Invalid frame ptr");

	const FrameDim& frame_dim = getFrameDim();
	if (!frame_info.frame_dim)
		frame_info.frame_dim = &frame_dim;
	else if (*frame_info.frame_dim != frame_dim)
		throw LIMA_HW_EXC(InvalidValue, "Invalid frame dim");

	if (frame_info.valid_pixels == 0)
		frame_info.valid_pixels = Point(frame_dim.getSize()).getArea();

	m_info_list[buffer_nb] = frame_info;

	if (!m_fcb_act)
		return false;

	return HwFrameCallbackGen::newFrameReady(frame_info);
}

const FrameDim& StdBufferCbMgr::getFrameDim()
{
	return m_alloc_mgr.getFrameDim();
}

int StdBufferCbMgr::getNbBuffers()
{
	return m_alloc_mgr.getNbBuffers();
}

void *StdBufferCbMgr::getBufferPtr(int buffer_nb)
{
	return m_alloc_mgr.getBufferPtr(buffer_nb);
}

void StdBufferCbMgr::clearBuffer(int buffer_nb)
{
	m_alloc_mgr.clearBuffer(buffer_nb);
}

void StdBufferCbMgr::clearAllBuffers()
{
	m_alloc_mgr.clearAllBuffers();
}

void StdBufferCbMgr::getFrameInfo(int acq_frame_nb, HwFrameInfo& info)
{
	int buffer_nb = acq_frame_nb % getNbBuffers();
	if (m_info_list[buffer_nb].acq_frame_nb != acq_frame_nb)
		throw LIMA_HW_EXC(Error, "Frame not available");

	info = m_info_list[buffer_nb];
}

/*******************************************************************
 * \brief BufferCtrlMgr constructor
 *******************************************************************/

BufferCtrlMgr::BufferCtrlMgr(BufferCbMgr& acq_buffer_mgr)
	: m_nb_concat_frames(1), m_nb_acc_frames(1),
	  m_acq_buffer_mgr(acq_buffer_mgr), 
	  m_aux_buffer_mgr(m_aux_alloc_mgr)
{
	m_effect_buffer_mgr = &m_acq_buffer_mgr;
}

BufferCtrlMgr::~BufferCtrlMgr()
{
}

void BufferCtrlMgr::setFrameDim(const FrameDim& frame_dim)
{
	if (frame_dim != m_frame_dim)
		m_effect_buffer_mgr->releaseBuffers();

	m_frame_dim = frame_dim;
}

void BufferCtrlMgr::getFrameDim(FrameDim& frame_dim)
{
	frame_dim = m_frame_dim;
}

void BufferCtrlMgr::setNbBuffers(int nb_buffers)
{
	if (nb_buffers == m_effect_buffer_mgr->getNbBuffers())
		return;

	int max_nb_buffers;
	getMaxNbBuffers(max_nb_buffers);
	if (nb_buffers > max_nb_buffers)
		throw LIMA_HW_EXC(InvalidValue, "Too many buffers");

	m_effect_buffer_mgr->allocBuffers(nb_buffers, m_frame_dim);
}

void BufferCtrlMgr::getNbBuffers(int& nb_buffers)
{
	nb_buffers = m_effect_buffer_mgr->getNbBuffers();
}

void BufferCtrlMgr::setNbConcatFrames(int nb_concat_frames)
{
	if (nb_concat_frames != m_nb_concat_frames)
		throw LIMA_HW_EXC(NotSupported, "Stripe concatenation is "
				  "not implemented yet");
}

void BufferCtrlMgr::getNbConcatFrames(int& nb_concat_frames)
{
	nb_concat_frames = m_nb_concat_frames;
}

void BufferCtrlMgr::setNbAccFrames(int nb_acc_frames)
{
	if (nb_acc_frames != m_nb_acc_frames)
		throw LIMA_HW_EXC(NotSupported, "Frame accumulation is "
				  "not implemented yet");
}

void BufferCtrlMgr::getNbAccFrames(int& nb_acc_frames)
{
	nb_acc_frames = m_nb_acc_frames;
}

void BufferCtrlMgr::getMaxNbBuffers(int& max_nb_buffers)
{
	max_nb_buffers = m_effect_buffer_mgr->getMaxNbBuffers(m_frame_dim);
}

void BufferCtrlMgr::registerFrameCallback(HwFrameCallback *frame_cb)
{
	m_effect_buffer_mgr->registerFrameCallback(frame_cb);
}

void BufferCtrlMgr::unregisterFrameCallback(HwFrameCallback *frame_cb)
{
	m_effect_buffer_mgr->unregisterFrameCallback(frame_cb);
}

BufferCbMgr& BufferCtrlMgr::getAcqBufferMgr()
{
	return m_acq_buffer_mgr;
}

void BufferCtrlMgr::setStartTimestamp(Timestamp start_ts)
{
	m_acq_buffer_mgr.setStartTimestamp(start_ts);
	if (m_effect_buffer_mgr != &m_acq_buffer_mgr)
		m_effect_buffer_mgr->setStartTimestamp(start_ts);
}

void BufferCtrlMgr::getStartTimestamp(Timestamp& start_ts)
{
	m_effect_buffer_mgr->getStartTimestamp(start_ts);
}

void BufferCtrlMgr::getFrameInfo(int acq_frame_nb, HwFrameInfoType& info)
{
	m_effect_buffer_mgr->getFrameInfo(acq_frame_nb, info);
}

void *BufferCtrlMgr::getBufferPtr(int buffer_nb)
{
	return m_effect_buffer_mgr->getBufferPtr(buffer_nb);
}

void *BufferCtrlMgr::getFramePtr(int acq_frame_nb)
{
	HwFrameInfoType info;
	getFrameInfo(acq_frame_nb, info);
	return info.frame_ptr;
}

BufferCtrlMgr::AcqMode BufferCtrlMgr::getAcqMode()
{
	if (m_nb_concat_frames > 1)
		return Concat;
	else if (m_nb_acc_frames > 1)
		return Acc;
	else
		return Normal;
}
