my_dir = g2d_legacy
legacy_obj = ${my_dir}/g2d_driver.o

CFLAGS_g2d_driver.o := -I$(src)/$(my_dir)
ifeq (${CONFIG_ARCH_SUN8IW11P1},y)
legacy_obj += ${my_dir}/g2d_bsp_sun8iw11.o
else
legacy_obj += ${my_dir}/g2d_bsp_v2.o
ccflags-y += -DG2D_V2X_SUPPORT
endif
