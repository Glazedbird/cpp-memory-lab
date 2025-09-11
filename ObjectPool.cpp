#include<cstddef>
#include<memory>
#include<iostream>
#include<new>   

template<std::size_t ObjectSize,
         std::size_t ObjectPerChunk,
         std::size_t Align = alignof(std::max_align_t)>
class ObjectPool {

    private:

        struct FreeNode
        {
            FreeNode* next = nullptr;
        };

        struct Chunk
        {
            Chunk* m_next = nullptr;
            std::byte* m_data = nullptr;
        };

        static constexpr std::size_t round_up(std::size_t n, std::size_t a) {
            return  (n + (a - 1)) & ~(a - 1);
        }

        static constexpr std::size_t m_BLOCKSIZE = round_up(
            ObjectSize > sizeof(FreeNode) ? ObjectSize : sizeof(FreeNode),
            Align > alignof(FreeNode) ? Align : alignof(FreeNode)
        );


        Chunk* chunks = nullptr;
        FreeNode* m_head = nullptr;

        void fill(std::size_t chunk_num = 1) {

            while(chunk_num --) {
                std::byte* bytes_p = static_cast<byte*>(::operator new[](m_BLOCKSIZE * ObjectPerChunk, std::align_val_t(Align)));
                Chunk* tmp_chunk = new Chunk{bytes_p, nullptr};
                for(std::size_t i = 0; i < ObjectPerChunk; i++){
                    FreeNode* freenode_p =  reinterpret_cast<FreeNode*>(bytes_p + (i * m_BLOCKSIZE));
                    //缺少construct_at(累了以后有机会再改吧)
                    freenode_p -> next = m_head;
                    m_head = freenode_p;
                }
                chunks -> m_next = tmp_chunk -> m_next;
                chunks = tmp_chunk;
            }
        }

        void release() {

            while(chunks) {
                std::byte* bytes = chunks -> m_data;
                ::operator delete[](bytes, std::align_val_t(Align));
                bytes = nullptr;
                Chunk* tmp = chunks;
                chunks = chunks -> m_next;
                delete tmp;
                tmp = nullptr;
            }
            m_head = nullptr;
        }

        
    public:
        
        ObjectPool(std::size_t chunk_num){
            fill(chunk_num);
        }
        
        ~ObjectPool(){
            release();
        }

        ObjectPool(const ObjectPool& other) = delete;
        ObjectPool& operator=(const ObjectPool& other) = delete;


        ObjectPool(ObjectPool&& rhs) 
        : chunks(rhs.chunks), m_head(rhs.m_head)
        {
            rhs.chunks = nullptr;
            rhs.m_head = nullptr;
        }

        ObjectPool& operator=(ObjectPool&& rhs) {
            if(this != & rhs) {
                release();
                m_head = rhs.m_head;
                chunks = rhs.chunks;
                rhs.chunks = nullptr;
                rhs.m_head = nullptr;
            }
            return *this;
        }

        void* allocate() {
            if (!m_head) fill(1);
            FreeNode* n = m_head;
            m_head = n->next;
            return n; 
        }

        void deallocate(void* p) noexcept {
            if (!p) return;
            auto* n = static_cast<FreeNode*>(p);
            n->next = m_head;
            m_head = n;
        }

        static constexpr std::size_t block_size() noexcept { return m_BLOCKSIZE; }

};