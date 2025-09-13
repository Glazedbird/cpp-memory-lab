#include<cstddef>
#include<memory>
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

        static_assert(Align && ((Align & (Align - 1)) == 0), "Align must be power of two");
        static_assert(ObjectPerChunk > 0, "ObjectPerChunk must be > 0");


        Chunk* chunks = nullptr;
        FreeNode* m_head = nullptr;

        void fill(std::size_t chunk_num = 1) {

            while(chunk_num --) {
                std::byte* bytes_p = static_cast<std::byte*>(::operator new[](m_BLOCKSIZE * ObjectPerChunk, std::align_val_t(Align)));
                // link new chunk to list; store its data pointer
                Chunk* tmp_chunk = new Chunk{nullptr, bytes_p};
                for(std::size_t i = 0; i < ObjectPerChunk; i++){
                    FreeNode* freenode_p =  reinterpret_cast<FreeNode*>(bytes_p + (i * m_BLOCKSIZE));
                    ::new(freenode_p) FreeNode();

                    freenode_p -> next = m_head;
                    m_head = freenode_p;
                }

                tmp_chunk -> m_next = chunks;
                chunks = tmp_chunk;
            }
        }

        void release() noexcept {

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
        
        ~ObjectPool() noexcept{
            release();
        }

        ObjectPool(const ObjectPool& other) = delete;
        ObjectPool& operator=(const ObjectPool& other) = delete;


        ObjectPool(ObjectPool&& rhs) noexcept 
        : chunks(rhs.chunks), m_head(rhs.m_head)
        {
            rhs.chunks = nullptr;
            rhs.m_head = nullptr;
        }

        ObjectPool& operator=(ObjectPool&& rhs) noexcept {
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
