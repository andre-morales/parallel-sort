# Decisões

1. **Uso do Radix Sort:** O melhor algoritmo de ordenação para o formato dos dados a serem ordenados. O Radix Sort tem um tempo de execução O(n*k), onde **k** é o tamanho da chave a ser utilizado. O algoritmo tem tempo de execução potencialmente **linear(!)** se o tamanho da chave for favorável.

Como as chaves são aleatórias seguindo uma distribuição relativamente uniforme, o radix sort aproveita bem o tamanho da chave.

Decidi utilizar duas passadas com o radix sort. A primeira passada ordena os 16 bits inferiores da chave, e a última passada ordena os 16 bits superiores da chave. Testes com 4 passadas de 8 bits se mostraram ligeiramente mais lentos.

2. **Ordenar chaves de ordenamento ao invés de ordenar os registros inteiros:**
Essa pŕatica mostrou-se consistentemente mais rápida que ordenar os registros inteiros em um Selection Sort trivialmente paralelizado. A diferença de tempo de execução foi de aproximadamente 60%.

Utilizar chaves de ordenamento gastam um tempo inicial para criar as chaves, e mais um tempo final para fazer o coalescimento dessas chaves de volta para o registro final. Dito isso, essas duas operações são **O(n)**, e trivialmente paralelizáveis.

3. **Não utilizar o máximo de threads possíveis:** Ou pelo menos nem sempre. A maioria dos sistemas utiliza hyperthreading. Cada core geralmente suporta 2 threads, mas essas threads competem por recursos. Por ser um algoritmo altamente otimizado, o tempo de execução acaba sendo limitado pela velocidade do barramento de memória, ao invés do poder de processamento puro do algoritmo, resultando em mais overhead. 

Além disso, o grande limitante do tempo de execução certamente será o disco. Principalmente para a saída. Mais threads tornam a sequência de saída imprevisível para o disco, gerando mais overhead.

Nos testes de performance, mesmo com vários tamanhos de arquivos diferentes e múltiplas configurações diferentes de limpeza (ou não) de cache, utilizar a maior quantidade de threads disponíveis **sempre** se mostrou pior do que não utilizar. Em um sistema de 12 threads, o _sweet spot_ se encontra entre 4 e 10 threads, onde quanto maior o tamanho do arquivo, mais benéfico é o uso de mais threads, mas nunca foi benéfico usar o máximo.


# Depuração de Desempenho na RAM

### 1) Merge Sort c/ Merge 2 por 2 paralelo

Work          |1 Th |2 Th |4 Th |8 Th |16 Th
--------------|----:|----:|----:|----:|----:
IO Overhead |88ms |60ms |40ms |36ms |40ms
Key Transform |100ms|64ms |47ms |42ms |42ms
Sort Keys     |160ms|130ms|110ms|94ms |84ms
Signaling     |164ms|130ms|112ms|97ms |92ms
Merging Halves|165ms|132ms|116ms|104ms|101ms
Full Sorting  |167ms|134ms|118ms|111ms|105ms

### 2) Radix Sort c/ Merge 2 por 2 paralelo
Work          |1 Th |2 Th |4 Th |8 Th |16 Th
--------------|----:|----:|----:|----:|----:
Full Sorting  |140ms|94ms|76ms|76ms|75ms

### 3) Radix Sort completamente paralelo
Work          |1 Th |2 Th |4 Th |8 Th |16 Th
--------------|----:|----:|----:|----:|----:
Full Sorting  |100ms|74ms|70ms|64ms|60ms