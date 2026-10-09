# Componente que compila o MESMO core/fsm_core.c analisado pelo ESBMC (sem copia).
set(CORE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../core")
idf_component_register(SRCS "${CORE_DIR}/fsm_core.c"
                       INCLUDE_DIRS "${CORE_DIR}"
                       LDFRAGMENTS "linker.lf")
