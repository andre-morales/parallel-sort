# Decisões
# Desempenho

### 1) Merge Sort c/ Merge 2 por 2 paralelo

Work          |1 Th |2 Th |4 Th |8 Th |16 Th
--------------|----:|----:|----:|----:|----:
Disk Overhead |88ms |60ms |40ms |36ms |40ms
Key Transform |100ms|64ms |47ms |42ms |42ms
Sort Keys     |160ms|130ms|110ms|94ms |84ms
Signaling     |164ms|130ms|112ms|97ms |92ms
Merging Halves|165ms|132ms|116ms|104ms|101ms
Full Sorting  |167ms|134ms|118ms|111ms|105ms

### 2) Radix Sort c/ Merge 2 por 2 paralelo
Work          |1 Th |2 Th |4 Th |8 Th |16 Th
--------------|----:|----:|----:|----:|----:
Full Sorting  |140ms|94ms|76ms|76ms|75ms

1. **Ordenar chaves de ordenamento ao invés de ordenar os registros inteiros.**
Essa pŕatica mostrou-se consistentemente mais rápida que ordenar os registros inteiros em um Selection Sort trivialmente paralelizado. A diferença de tempo de execução foi de aproximadamente 20%.

Utilizar chaves de ordenamento gastam um tempo inicial para criar as chaves, e mais um tempo final para fazer o coalescimento dessas chaves de volta para o registro final. Dito isso, essas duas operações são **O(n)**, e trivialmente paralelizáveis.

2. **Usar chaves de 64-bits.** Mesmo ocupando mais espaço, o alinhamento na memória afeta de forma significante a performance do algoritmo. Utilizando uma chave de ordenação de 16 bytes ao invés de 12 garante acessos bem alinhados na memória. Ajudando a performance da cache.