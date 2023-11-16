int deepspeed_aio_handle_t::sync_pread(torch::Tensor &buffer, const char *filename)
{
    _num_pending_ops++;
    layerVecMap& layerVecMapMgr = layerVecMap::getInstance();
    void* data_ptr = buffer.data_ptr();
    unsigned long long data_size = buffer.numel() * sizeof(float);
    return layerVecMapMgr.parsefromFileName(filename, 0, data_ptr, 0);
}

int deepspeed_aio_handle_t::sync_pwrite(const torch::Tensor &buffer, const char *filename)
{
    _num_pending_ops++;
    layerVecMap& layerVecMapMgr = layerVecMap::getInstance();
    void* data_ptr = buffer.data_ptr();
    unsigned long long data_size = buffer.numel() * sizeof(float);
    return layerVecMapMgr.parsefromFileName(filename, 1, data_ptr, 0);
}

int deepspeed_aio_handle_t::async_pread(torch::Tensor &buffer, const char *filename)
{
    _num_pending_ops++;
    layerVecMap& layerVecMapMgr = layerVecMap::getInstance();
    void* data_ptr = buffer.data_ptr();
    unsigned long long data_size = buffer.numel() * sizeof(float);
    return layerVecMapMgr.parsefromFileName(filename, 0, data_ptr, 0);
}

int deepspeed_aio_handle_t::async_pwrite(const torch::Tensor &buffer, const char *filename)
{
    _num_pending_ops++;
    layerVecMap& layerVecMapMgr = layerVecMap::getInstance();
    void* data_ptr = buffer.data_ptr();
    unsigned long long data_size = buffer.numel() * sizeof(float);
    return layerVecMapMgr.parsefromFileName(filename, 1, data_ptr, 0);
}

int deepspeed_aio_handle_t::wait()
{
    auto num_completed_ops = _num_pending_ops;
    _num_pending_ops = 0;
    return num_completed_ops;
}