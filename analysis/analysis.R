library(ggplot2)
sizes = c(1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024)

readRaw <- function(tableName) {
	table = read.csv2(tableName, sep=",")
	table$mean = as.numeric(table$mean) * 1000
	table$parameter_threads = as.numeric(table$parameter_threads)
	
	for (a in sizes) {
		rows = subset(table, parameter_size==a)
		
		show(
			ggplot(rows, aes(x = parameter_threads, y = mean)) +
				scale_x_continuous(breaks = scales::breaks_pretty(n = 12)) + 
				scale_y_continuous(breaks = scales::breaks_pretty(n = 10)) + 
				geom_line() +
				geom_point() +
				labs(title=paste(tableName, a, "MB"), x="Threads", y="Time(s)")
		)
		
		#plot(rows$parameter_threads, rows$mean, type="o", xlab="Threads", ylab="Tempo(ms)", main=)
	}
	return(table)
}

dataFrame <<- readRaw("linux_disk_disk.csv")