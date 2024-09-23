#include "esp_log.h"
#include "nvs_config.h"
#include "serial.h"
#include "system.h"
#include "utils.h"
#include <string.h>
#include "boards/board.h"

static const char *TAG = "asic_result";

extern int stratum_sock;

bm_job *clone_bm_job(bm_job *src)
{
    bm_job *dst = (bm_job *) malloc(sizeof(bm_job));

    // copy all
    memcpy(dst, src, sizeof(bm_job));

    // copy strings
    dst->extranonce2 = strdup(src->extranonce2);
    dst->jobid = strdup(src->jobid);

    return dst;
}

void ASIC_result_task(void *pvParameters)
{
    char *user = nvs_config_get_string(NVS_CONFIG_STRATUM_USER, STRATUM_USER);

    while (1) {
        //ESP_LOGI("Memory", "%lu", esp_get_free_heap_size()); test
        task_result asic_result;

        // get the result
        if (!board_asic_proccess_work(&asic_result)) {
            continue;
        }

        if (asic_result.is_reg_resp) {
            // TODO evaluate response
            continue;
        }

        uint8_t asic_job_id = asic_result.job_id;

        // check if we have a job with this job id
        pthread_mutex_lock(&ASIC_TASK_MODULE.valid_jobs_lock);
        if (ASIC_TASK_MODULE.valid_jobs[asic_job_id] == 0) {
            ESP_LOGI(TAG, "Invalid job id found, 0x%02X", asic_job_id);
            pthread_mutex_unlock(&ASIC_TASK_MODULE.valid_jobs_lock);
            continue;
        }
        // we create a clone because we would lock during verification and stratum submit
        // what is potentially bad
        bm_job *job = clone_bm_job(ASIC_TASK_MODULE.active_jobs[asic_job_id]);
        pthread_mutex_unlock(&ASIC_TASK_MODULE.valid_jobs_lock);

        // now we have the original job and can `or` the version
        asic_result.rolled_version |= job->version;

        // check the nonce difficulty
        double nonce_diff = test_nonce_value(job, asic_result.nonce, asic_result.rolled_version);

        // log the ASIC response
        ESP_LOGI(TAG, "Job ID: %02X AsicNr: %d Ver: %08" PRIX32 " Nonce %08" PRIX32 " diff %.1f of %ld.", asic_job_id,
                 asic_result.asic_nr, asic_result.rolled_version, asic_result.nonce, nonce_diff, job->asic_diff);

        if (nonce_diff > job->pool_diff) {
            STRATUM_V1_submit_share(stratum_sock, user, job->jobid, job->extranonce2, job->ntime, asic_result.nonce,
                                    asic_result.rolled_version ^ job->version);
        }

        if (nonce_diff > job->asic_diff) {
            SYSTEM_notify_found_nonce((double) job->asic_diff, asic_result.asic_nr);
        }

        SYSTEM_check_for_best_diff(nonce_diff, asic_job_id);

        free_bm_job(job);
    }
}
