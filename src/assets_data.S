.section .rodata
.balign 4
.global ez_atlas_blob
ez_atlas_blob:
.incbin "assets/atlas.bin"
.global ez_atlas_blob_end
ez_atlas_blob_end:

.balign 4
.global ez_art_blob
ez_art_blob:
.incbin "assets/art.bin"
.global ez_art_blob_end
ez_art_blob_end:

#ifndef __mips__
.section .note.GNU-stack,"",@progbits
#endif
